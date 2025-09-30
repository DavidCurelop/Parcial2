// Programa de procesamiento de imágenes en C para principiantes en Linux.
// QUÉ: Procesa imágenes PNG (escala de grises o RGB) usando matrices, con soporte
// para carga, visualización, guardado y ajuste de brillo concurrente.
// CÓMO: Usa stb_image.h para cargar PNG y stb_image_write.h para guardar PNG,
// con hilos POSIX (pthread) para el procesamiento paralelo del brillo.
// POR QUÉ: Diseñado para enseñar manejo de matrices, concurrencia y gestión de
// memoria en C, manteniendo simplicidad y robustez para principiantes.
// Dependencias: Descarga stb_image.h y stb_image_write.h desde
// https://github.com/nothings/stb
//   wget https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
//   wget https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
//
// Compilar: gcc -o img img_base.c -pthread -lm
// Ejecutar: ./img [ruta_imagen.png]

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// QUÉ: Incluir bibliotecas stb para cargar y guardar imágenes PNG.
// CÓMO: stb_image.h lee PNG/JPG a memoria; stb_image_write.h escribe PNG.
// POR QUÉ: Son bibliotecas de un solo archivo, simples y sin dependencias externas.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// QUÉ: Estructura para almacenar la imagen (ancho, alto, canales, píxeles).
// CÓMO: Usa matriz 3D para píxeles (alto x ancho x canales), donde canales es
// 1 (grises) o 3 (RGB). Píxeles son unsigned char (0-255).
// POR QUÉ: Permite manejar tanto grises como color, con memoria dinámica para
// flexibilidad y evitar desperdicio.
typedef struct
{
    int ancho;                // Ancho de la imagen en píxeles
    int alto;                 // Alto de la imagen en píxeles
    int canales;              // 1 (escala de grises) o 3 (RGB)
    unsigned char ***pixeles; // Matriz 3D: [alto][ancho][canales]
} ImagenInfo;

// QUÉ: Liberar memoria asignada para la imagen.
// CÓMO: Libera cada fila y canal de la matriz 3D, luego el arreglo de filas y
// reinicia la estructura.
// POR QUÉ: Evita fugas de memoria, esencial en C para manejar recursos manualmente.
void liberarImagen(ImagenInfo *info)
{
    if (info->pixeles)
    {
        for (int y = 0; y < info->alto; y++)
        {
            for (int x = 0; x < info->ancho; x++)
            {
                free(info->pixeles[y][x]); // Liberar canales por píxel
            }
            free(info->pixeles[y]); // Liberar fila
        }
        free(info->pixeles); // Liberar arreglo de filas
        info->pixeles = NULL;
    }
    info->ancho = 0;
    info->alto = 0;
    info->canales = 0;
}

// QUÉ: Cargar una imagen PNG desde un archivo.
// CÓMO: Usa stbi_load para leer el archivo, detecta canales (1 o 3), y convierte
// los datos a una matriz 3D (alto x ancho x canales).
// POR QUÉ: La matriz 3D es intuitiva para principiantes y permite procesar
// píxeles y canales individualmente.
int cargarImagen(const char *ruta, ImagenInfo *info)
{
    int canales;
    // QUÉ: Cargar imagen con formato original (0 canales = usar formato nativo).
    // CÓMO: stbi_load lee el archivo y llena ancho, alto y canales.
    // POR QUÉ: Respetar el formato original asegura que grises o RGB se mantengan.
    unsigned char *datos = stbi_load(ruta, &info->ancho, &info->alto, &canales, 0);
    if (!datos)
    {
        fprintf(stderr, "Error al cargar imagen: %s\n", ruta);
        return 0;
    }
    info->canales = (canales == 1 || canales == 3) ? canales : 1; // Forzar 1 o 3

    // QUÉ: Asignar memoria para matriz 3D.
    // CÓMO: Asignar alto filas, luego ancho columnas por fila, luego canales por píxel.
    // POR QUÉ: Estructura clara y flexible para grises (1 canal) o RGB (3 canales).
    info->pixeles = (unsigned char ***)malloc(info->alto * sizeof(unsigned char **));
    if (!info->pixeles)
    {
        fprintf(stderr, "Error de memoria al asignar filas\n");
        stbi_image_free(datos);
        return 0;
    }
    for (int y = 0; y < info->alto; y++)
    {
        info->pixeles[y] = (unsigned char **)malloc(info->ancho * sizeof(unsigned char *));
        if (!info->pixeles[y])
        {
            fprintf(stderr, "Error de memoria al asignar columnas\n");
            liberarImagen(info);
            stbi_image_free(datos);
            return 0;
        }
        for (int x = 0; x < info->ancho; x++)
        {
            info->pixeles[y][x] = (unsigned char *)malloc(info->canales * sizeof(unsigned char));
            if (!info->pixeles[y][x])
            {
                fprintf(stderr, "Error de memoria al asignar canales\n");
                liberarImagen(info);
                stbi_image_free(datos);
                return 0;
            }
            // Copiar píxeles a matriz 3D
            for (int c = 0; c < info->canales; c++)
            {
                info->pixeles[y][x][c] = datos[(y * info->ancho + x) * info->canales + c];
            }
        }
    }

    stbi_image_free(datos); // Liberar buffer de stb
    printf("Imagen cargada: %dx%d, %d canales (%s)\n", info->ancho, info->alto,
           info->canales, info->canales == 1 ? "grises" : "RGB");
    return 1;
}

// QUÉ: Mostrar la matriz de píxeles (primeras 10 filas).
// CÓMO: Imprime los valores de los píxeles, agrupando canales por píxel (grises o RGB).
// POR QUÉ: Ayuda a visualizar la matriz para entender la estructura de datos.
void mostrarMatriz(const ImagenInfo *info)
{
    if (!info->pixeles)
    {
        printf("No hay imagen cargada.\n");
        return;
    }
    printf("Matriz de la imagen (primeras 10 filas):\n");
    for (int y = 0; y < info->alto && y < 10; y++)
    {
        for (int x = 0; x < info->ancho; x++)
        {
            if (info->canales == 1)
            {
                printf("%3u ", info->pixeles[y][x][0]); // Escala de grises
            }
            else
            {
                printf("(%3u,%3u,%3u) ", info->pixeles[y][x][0], info->pixeles[y][x][1],
                       info->pixeles[y][x][2]); // RGB
            }
        }
        printf("\n");
    }
    if (info->alto > 10)
    {
        printf("... (más filas)\n");
    }
}

// QUÉ: Guardar la matriz como PNG (grises o RGB).
// CÓMO: Aplana la matriz 3D a 1D y usa stbi_write_png con el número de canales correcto.
// POR QUÉ: Respeta el formato original (grises o RGB) para consistencia.
int guardarPNG(const ImagenInfo *info, const char *rutaSalida)
{
    if (!info->pixeles)
    {
        fprintf(stderr, "No hay imagen para guardar.\n");
        return 0;
    }

    // QUÉ: Aplanar matriz 3D a 1D para stb.
    // CÓMO: Copia píxeles en orden [y][x][c] a un arreglo plano.
    // POR QUÉ: stb_write_png requiere datos contiguos.
    unsigned char *datos1D = (unsigned char *)malloc(info->ancho * info->alto * info->canales);
    if (!datos1D)
    {
        fprintf(stderr, "Error de memoria al aplanar imagen\n");
        return 0;
    }
    for (int y = 0; y < info->alto; y++)
    {
        for (int x = 0; x < info->ancho; x++)
        {
            for (int c = 0; c < info->canales; c++)
            {
                datos1D[(y * info->ancho + x) * info->canales + c] = info->pixeles[y][x][c];
            }
        }
    }

    // QUÉ: Guardar como PNG.
    // CÓMO: Usa stbi_write_png con los canales de la imagen original.
    // POR QUÉ: Mantiene el formato (grises o RGB) de la entrada.
    int resultado = stbi_write_png(rutaSalida, info->ancho, info->alto, info->canales,
                                   datos1D, info->ancho * info->canales);
    free(datos1D);
    if (resultado)
    {
        printf("Imagen guardada en: %s (%s)\n", rutaSalida,
               info->canales == 1 ? "grises" : "RGB");
        return 1;
    }
    else
    {
        fprintf(stderr, "Error al guardar PNG: %s\n", rutaSalida);
        return 0;
    }
}

// QUÉ: Estructura para pasar datos al hilo de ajuste de brillo.
// CÓMO: Contiene matriz, rango de filas, ancho, canales y delta de brillo.
// POR QUÉ: Los hilos necesitan datos específicos para procesar en paralelo.
typedef struct
{
    unsigned char ***pixeles;
    int inicio;
    int fin;
    int ancho;
    int canales;
    int delta;
} BrilloArgs;

// QUÉ: Ajustar brillo en un rango de filas (para hilos).
// CÓMO: Suma delta a cada canal de cada píxel, con clamp entre 0-255.
// POR QUÉ: Procesa píxeles en paralelo para demostrar concurrencia.
void *ajustarBrilloHilo(void *args)
{
    BrilloArgs *bArgs = (BrilloArgs *)args;
    for (int y = bArgs->inicio; y < bArgs->fin; y++)
    {
        for (int x = 0; x < bArgs->ancho; x++)
        {
            for (int c = 0; c < bArgs->canales; c++)
            {
                int nuevoValor = bArgs->pixeles[y][x][c] + bArgs->delta;
                bArgs->pixeles[y][x][c] = (unsigned char)(nuevoValor < 0 ? 0 : (nuevoValor > 255 ? 255 : nuevoValor));
            }
        }
    }
    return NULL;
}

// QUÉ: Ajustar brillo de la imagen usando múltiples hilos.
// CÓMO: Divide las filas entre 2 hilos, pasa argumentos y espera con join.
// POR QUÉ: Usa concurrencia para acelerar el procesamiento y enseñar hilos.
void ajustarBrilloConcurrente(ImagenInfo *info, int delta)
{
    if (!info->pixeles)
    {
        printf("No hay imagen cargada.\n");
        return;
    }

    const int numHilos = 2; // QUÉ: Número fijo de hilos para simplicidad.
    pthread_t hilos[numHilos];
    BrilloArgs args[numHilos];
    int filasPorHilo = (int)ceil((double)info->alto / numHilos);

    // QUÉ: Configurar y lanzar hilos.
    // CÓMO: Asigna rangos de filas a cada hilo y pasa datos.
    // POR QUÉ: Divide el trabajo para procesar en paralelo.
    for (int i = 0; i < numHilos; i++)
    {
        args[i].pixeles = info->pixeles;
        args[i].inicio = i * filasPorHilo;
        args[i].fin = (i + 1) * filasPorHilo < info->alto ? (i + 1) * filasPorHilo : info->alto;
        args[i].ancho = info->ancho;
        args[i].canales = info->canales;
        args[i].delta = delta;
        if (pthread_create(&hilos[i], NULL, ajustarBrilloHilo, &args[i]) != 0)
        {
            fprintf(stderr, "Error al crear hilo %d\n", i);
            return;
        }
    }

    // QUÉ: Esperar a que los hilos terminen.
    // CÓMO: Usa pthread_join para sincronizar.
    // POR QUÉ: Garantiza que todos los píxeles se procesen antes de continuar.
    for (int i = 0; i < numHilos; i++)
    {
        pthread_join(hilos[i], NULL);
    }
    printf("Brillo ajustado concurrentemente con %d hilos (%s).\n", numHilos,
           info->canales == 1 ? "grises" : "RGB");
}

// QUÉ: Mostrar el menú interactivo.
// CÓMO: Imprime opciones y espera entrada del usuario.
// POR QUÉ: Proporciona una interfaz simple para interactuar con el programa.
void mostrarMenu()
{
    printf("\n--- Plataforma de Edición de Imágenes ---\n");
    printf("1. Cargar imagen PNG\n2. Mostrar matriz de píxeles\n3. Guardar como PNG\n4. Ajustar brillo concurrente\n5. Convolución Gaussiana\n6. Detección de bordes (Sobel)\n7. Rotar imagen\n8. Redimensionar\n9. Salir\nOpción: ");
}

// QUÉ: Función principal que controla el flujo del programa.
// CÓMO: Maneja entrada CLI, ejecuta el menú en bucle y llama funciones según opción.
// POR QUÉ: Centraliza la lógica y asegura limpieza al salir.
int main(int argc, char *argv[])
{
    ImagenInfo imagen = {0, 0, 0, NULL}; // Inicializar estructura
    char ruta[256] = {0};                // Buffer para ruta de archivo

    // QUÉ: Cargar imagen desde CLI si se pasa.
    // CÓMO: Copia argv[1] y llama cargarImagen.
    // POR QUÉ: Permite ejecución directa con ./img imagen.png.
    if (argc > 1)
    {
        strncpy(ruta, argv[1], sizeof(ruta) - 1);
        if (!cargarImagen(ruta, &imagen))
        {
            return EXIT_FAILURE;
        }
    }

    int opcion;
    while (1)
    {
        mostrarMenu();
        // QUÉ: Leer opción del usuario.
        // CÓMO: Usa scanf y limpia el buffer para evitar bucles infinitos.
        // POR QUÉ: Manejo robusto de entrada evita errores comunes.
        if (scanf("%d", &opcion) != 1)
        {
            while (getchar() != '\n')
                ;
            printf("Entrada inválida.\n");
            continue;
        }
        while (getchar() != '\n')
            ; // Limpiar buffer

        switch (opcion)
        {
        case 1:
        { // Cargar imagen
            printf("Ingresa la ruta del archivo PNG: ");
            if (fgets(ruta, sizeof(ruta), stdin) == NULL)
            {
                printf("Error al leer ruta.\n");
                continue;
            }
            ruta[strcspn(ruta, "\n")] = 0; // Eliminar salto de línea
            liberarImagen(&imagen);        // Liberar imagen previa
            if (!cargarImagen(ruta, &imagen))
            {
                continue;
            }
            break;
        }
        case 2: // Mostrar matriz
            mostrarMatriz(&imagen);
            break;
        case 3:
        { // Guardar PNG
            char salida[256];
            printf("Nombre del archivo PNG de salida: ");
            if (fgets(salida, sizeof(salida), stdin) == NULL)
            {
                printf("Error al leer ruta.\n");
                continue;
            }
            salida[strcspn(salida, "\n")] = 0;
            guardarPNG(&imagen, salida);
            break;
        }
        case 4:
        { // Ajustar brillo
            int delta;
            printf("Valor de ajuste de brillo (+ para más claro, - para más oscuro): ");
            if (scanf("%d", &delta) != 1)
            {
                while (getchar() != '\n')
                    ;
                printf("Entrada inválida.\n");
                continue;
            }
            while (getchar() != '\n')
                ;
            ajustarBrilloConcurrente(&imagen, delta);
            break;
        }
        case 5:
        {
            int k, h;
            float sigma;
            scanf("%d", &k);
            scanf("%f", &sigma);
            scanf("%d", &h);
            while (getchar() != '\n')
                ;
            aplicarConvolucionGaussiana(&imagen, k, sigma, h);
            break;
        }
        case 6:
        {
            int h;
            scanf("%d", &h);
            while (getchar() != '\n')
                ;
            aplicarSobel(&imagen, h);
            break;
        }
        case 7:
        {
            float ang;
            int h;
            scanf("%f", &ang);
            scanf("%d", &h);
            while (getchar() != '\n')
                ;
            rotarImagen(&imagen, ang, h);
            break;
        }
        case 8:
        {
            int nw, nh, h;
            scanf("%d", &nw);
            scanf("%d", &nh);
            scanf("%d", &h);
            while (getchar() != '\n')
                ;
            redimensionarBilineal(&imagen, nw, nh, h);
            break;
        }
        case 9: // Salir
            liberarImagen(&imagen);
            printf("¡Adiós!\n");
            return EXIT_SUCCESS;
        default:
            printf("Opción inválida.\n");
        }
    }
    liberarImagen(&imagen);
    return EXIT_SUCCESS;
}

static inline unsigned char clampU8(int v)
{
    return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static unsigned char ***allocPix3D(int ancho, int alto, int canales)
{
    unsigned char ***P = (unsigned char ***)malloc(alto * sizeof(unsigned char **));
    for (int y = 0; y < alto; y++)
    {
        P[y] = (unsigned char **)malloc(ancho * sizeof(unsigned char *));
        for (int x = 0; x < ancho; x++)
        {
            P[y][x] = (unsigned char *)malloc(canales);
        }
    }
    return P;
}

static void freePix3D(unsigned char ***P, int ancho, int alto)
{
    for (int y = 0; y < alto; y++)
    {
        for (int x = 0; x < ancho; x++)
            free(P[y][x]);
        free(P[y]);
    }
    free(P);
}

static void copyImageData(unsigned char ***dst, unsigned char ***src, int ancho, int alto, int canales)
{
    for (int y = 0; y < alto; y++)
        for (int x = 0; x < ancho; x++)
            for (int c = 0; c < canales; c++)
                dst[y][x][c] = src[y][x][c];
}

typedef struct
{
    ImagenInfo *info;
    unsigned char ***src;
    unsigned char ***dst;
    float *kernel;
    int k;
    int inicio;
    int fin;
} ConvArgs;

static void *hiloConvolucion(void *a)
{
    ConvArgs *A = (ConvArgs *)a;
    int half = A->k / 2, W = A->info->ancho, H = A->info->alto, C = A->info->canales;
    for (int y = A->inicio; y < A->fin; y++)
        for (int x = 0; x < W; x++)
            for (int c = 0; c < C; c++)
            {
                float acc = 0;
                for (int ky = -half; ky <= half; ky++)
                {
                    int sy = y + ky;
                    if (sy < 0)
                        sy = 0;
                    if (sy >= H)
                        sy = H - 1;
                    for (int kx = -half; kx <= half; kx++)
                    {
                        int sx = x + kx;
                        if (sx < 0)
                            sx = 0;
                        if (sx >= W)
                            sx = W - 1;
                        acc += A->kernel[(ky + half) * A->k + (kx + half)] * A->src[sy][sx][c];
                    }
                }
                A->dst[y][x][c] = clampU8((int)roundf(acc));
            }
    return NULL;
}

static void makeGaussianKernel(float *K, int k, float sigma)
{
    int half = k / 2;
    float sum = 0, s2 = 2 * sigma * sigma;
    for (int y = -half; y <= half; y++)
        for (int x = -half; x <= half; x++)
        {
            float v = expf(-(x * x + y * y) / s2);
            K[(y + half) * k + (x + half)] = v;
            sum += v;
        }
    for (int i = 0; i < k * k; i++)
        K[i] /= sum;
}

void aplicarConvolucionGaussiana(ImagenInfo *info, int k, float sigma, int nh)
{
    if (!info->pixeles)
        return;
    unsigned char ***src = allocPix3D(info->ancho, info->alto, info->canales);
    copyImageData(src, info->pixeles, info->ancho, info->alto, info->canales);
    unsigned char ***dst = allocPix3D(info->ancho, info->alto, info->canales);
    float *K = (float *)malloc(k * k * sizeof(float));
    makeGaussianKernel(K, k, sigma);
    pthread_t th[nh];
    ConvArgs args[nh];
    int filas = (info->alto + nh - 1) / nh;
    for (int i = 0; i < nh; i++)
    {
        args[i] = (ConvArgs){info, src, dst, K, k, i * filas, (i + 1) * filas < info->alto ? (i + 1) * filas : info->alto};
        pthread_create(&th[i], NULL, hiloConvolucion, &args[i]);
    }
    for (int i = 0; i < nh; i++)
        pthread_join(th[i], NULL);
    freePix3D(info->pixeles, info->ancho, info->alto);
    info->pixeles = dst;
    freePix3D(src, info->ancho, info->alto);
    free(K);
}

typedef struct
{
    ImagenInfo *info;
    unsigned char ***src;
    unsigned char ***dst;
    int inicio;
    int fin;
} SobelArgs;

static void *hiloSobel(void *a)
{
    SobelArgs *A = (SobelArgs *)a;
    int W = A->info->ancho, H = A->info->alto;
    const int Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}}, Gy[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
    for (int y = A->inicio; y < A->fin; y++)
        for (int x = 0; x < W; x++)
        {
            float sx = 0, sy = 0;
            for (int ky = -1; ky <= 1; ky++)
            {
                int yy = y + ky;
                if (yy < 0)
                    yy = 0;
                if (yy >= H)
                    yy = H - 1;
                for (int kx = -1; kx <= 1; kx++)
                {
                    int xx = x + kx;
                    if (xx < 0)
                        xx = 0;
                    if (xx >= W)
                        xx = W - 1;
                    float g = A->info->canales == 1 ? A->src[yy][xx][0] : (A->src[yy][xx][0] + A->src[yy][xx][1] + A->src[yy][xx][2]) / 3.0f;
                    sx += g * Gx[ky + 1][kx + 1];
                    sy += g * Gy[ky + 1][kx + 1];
                }
            }
            float m = sqrtf(sx * sx + sy * sy);
            if (m > 255)
                m = 255;
            A->dst[y][x][0] = (unsigned char)m;
        }
    return NULL;
}

void aplicarSobel(ImagenInfo *info, int nh)
{
    if (!info->pixeles)
        return;
    unsigned char ***src = allocPix3D(info->ancho, info->alto, info->canales);
    copyImageData(src, info->pixeles, info->ancho, info->alto, info->canales);
    unsigned char ***dst = allocPix3D(info->ancho, info->alto, 1);
    pthread_t th[nh];
    SobelArgs args[nh];
    int filas = (info->alto + nh - 1) / nh;
    for (int i = 0; i < nh; i++)
    {
        args[i] = (SobelArgs){info, src, dst, i * filas, (i + 1) * filas < info->alto ? (i + 1) * filas : info->alto};
        pthread_create(&th[i], NULL, hiloSobel, &args[i]);
    }
    for (int i = 0; i < nh; i++)
        pthread_join(th[i], NULL);
    freePix3D(info->pixeles, info->ancho, info->alto);
    info->pixeles = dst;
    info->canales = 1;
    freePix3D(src, info->ancho, info->alto);
}

typedef struct
{
    ImagenInfo *src;
    unsigned char ***dst;
    int Wn, Hn;
    float cosA, sinA, cx, cy, cnx, cny;
    int inicio, fin;
} RotArgs;

static void sampleBilinear(const ImagenInfo *src, float xf, float yf, unsigned char *out)
{
    int W = src->ancho, H = src->alto, C = src->canales;
    if (xf < 0 || yf < 0 || xf > W - 1 || yf > H - 1)
    {
        for (int c = 0; c < C; c++)
            out[c] = 0;
        return;
    }
    int x0 = (int)xf, y0 = (int)yf, x1 = x0 + 1, y1 = y0 + 1;
    if (x1 >= W)
        x1 = W - 1;
    if (y1 >= H)
        y1 = H - 1;
    float dx = xf - x0, dy = yf - y0;
    for (int c = 0; c < C; c++)
    {
        float v00 = src->pixeles[y0][x0][c], v10 = src->pixeles[y0][x1][c], v01 = src->pixeles[y1][x0][c], v11 = src->pixeles[y1][x1][c];
        float v0 = v00 * (1 - dx) + v10 * dx, v1 = v01 * (1 - dx) + v11 * dx, v = v0 * (1 - dy) + v1 * dy;
        out[c] = clampU8((int)(v + 0.5f));
    }
}

static void *hiloRotar(void *a)
{
    RotArgs *A = (RotArgs *)a;
    int C = A->src->canales;
    for (int y = A->inicio; y < A->fin; y++)
        for (int x = 0; x < A->Wn; x++)
        {
            float xn = x - A->cnx, yn = y - A->cny;
            float xs = A->cosA * xn + A->sinA * yn + A->cx, ys = -A->sinA * xn + A->cosA * yn + A->cy;
            sampleBilinear(A->src, xs, ys, A->dst[y][x]);
        }
    return NULL;
}

void rotarImagen(ImagenInfo *info, float ang, int nh)
{
    if (!info->pixeles)
        return;
    float rad = ang * M_PI / 180.0f, cosA = cosf(rad), sinA = sinf(rad);
    int W = info->ancho, H = info->alto, C = info->canales;
    float corners[4][2] = {{-W / 2.f, -H / 2.f}, {W / 2.f, -H / 2.f}, {W / 2.f, H / 2.f}, {-W / 2.f, H / 2.f}};
    float minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
    for (int i = 0; i < 4; i++)
    {
        float xr = cosA * corners[i][0] + sinA * corners[i][1], yr = -sinA * corners[i][0] + cosA * corners[i][1];
        if (xr < minx)
            minx = xr;
        if (xr > maxx)
            maxx = xr;
        if (yr < miny)
            miny = yr;
        if (yr > maxy)
            maxy = yr;
    }
    int Wn = (int)ceilf(maxx - minx), Hn = (int)ceilf(maxy - miny);
    unsigned char ***dst = allocPix3D(Wn, Hn, C);
    pthread_t th[nh];
    RotArgs args[nh];
    int filas = (Hn + nh - 1) / nh;
    RotArgs base = {info, dst, Wn, Hn, cosA, sinA, (W - 1) / 2.0f, (H - 1) / 2.0f, (Wn - 1) / 2.0f, (Hn - 1) / 2.0f, 0, 0};
    for (int i = 0; i < nh; i++)
    {
        args[i] = base;
        args[i].inicio = i * filas;
        args[i].fin = (i + 1) * filas < Hn ? (i + 1) * filas : Hn;
        pthread_create(&th[i], NULL, hiloRotar, &args[i]);
    }
    for (int i = 0; i < nh; i++)
        pthread_join(th[i], NULL);
    freePix3D(info->pixeles, info->ancho, info->alto);
    info->pixeles = dst;
    info->ancho = Wn;
    info->alto = Hn;
    info->canales = C;
}

typedef struct
{
    ImagenInfo *src;
    unsigned char ***dst;
    int Wn, Hn;
    float sx, sy;
    int inicio, fin;
} ResizeArgs;

static void *hiloResize(void *a)
{
    ResizeArgs *A = (ResizeArgs *)a;
    int C = A->src->canales;
    for (int y = A->inicio; y < A->fin; y++)
    {
        float yf = (y + 0.5f) * A->sy - 0.5f;
        for (int x = 0; x < A->Wn; x++)
        {
            float xf = (x + 0.5f) * A->sx - 0.5f;
            sampleBilinear(A->src, xf, yf, A->dst[y][x]);
        }
    }
    return NULL;
}

void redimensionarBilineal(ImagenInfo *info, int Wn, int Hn, int nh)
{
    if (!info->pixeles)
        return;
    unsigned char ***dst = allocPix3D(Wn, Hn, info->canales);
    float sx = (float)info->ancho / Wn, sy = (float)info->alto / Hn;
    pthread_t th[nh];
    ResizeArgs args[nh];
    int filas = (Hn + nh - 1) / nh;
    for (int i = 0; i < nh; i++)
    {
        args[i] = (ResizeArgs){info, dst, Wn, Hn, sx, sy, i * filas, (i + 1) * filas < Hn ? (i + 1) * filas : Hn};
        pthread_create(&th[i], NULL, hiloResize, &args[i]);
    }
    for (int i = 0; i < nh; i++)
        pthread_join(th[i], NULL);
    freePix3D(info->pixeles, info->ancho, info->alto);
    info->pixeles = dst;
    info->ancho = Wn;
    info->alto = Hn;
}