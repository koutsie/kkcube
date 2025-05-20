#ifdef PROFILE_BUILD
#define LOOP_LIMIT 2048
#else
#define LOOP_LIMIT -1
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <termios.h>
#include <getopt.h>
#include <immintrin.h>

// Common FPS limits:

// 4166.666 - 280fps
// 6944.444 - 144fps
// 8333.333 - 120fps
#define DEFAULT_FRAMETIME 11111 // 90~fps - Default
// 16.66666 - 60fps
// 41666.66 - 24fps

typedef struct
{
    size_t rows;
    size_t columns;
} dimension_t;

typedef struct
{
    dimension_t size;
    float *data;
} mat_t;

typedef struct
{
    size_t a;
    size_t b;
} edge_t;

void clear_screen()
{
    printf("\033[2J");
}

void get_cursor_pos(size_t *r, size_t *c)
{
    // Save terminal attrs...
    struct termios attrs;
    tcgetattr(STDIN_FILENO, &attrs);

    // Set terminal to raw mode
    struct termios raw;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    // Request cursor position
    printf("\033[6n");

    // Read response...
    const size_t RESPONSE_LENGTH = 100;
    char response[RESPONSE_LENGTH];
    int in;
    size_t i = 0;
    while ((in = fgetc(stdin)) != 'R' && i < RESPONSE_LENGTH - 1)
    {
        response[i++] = (char)in;
    }
    response[i] = '\0';
    printf("%s\n", response);

    // row begins after '['
    char *row = strchr(response, '[') + 1;

    // column begins after ';'
    char *column = strchr(row, ';') + 1;

    // Mark the end of the row text.
    *(column - 1) = '\0';

    *r = atol(row);
    *c = atol(column);

    // Restore terminal attrs
    tcsetattr(STDIN_FILENO, TCSANOW, &attrs);
}

void get_terminal_size(size_t *r, size_t *c)
{
    // Save cursor pos
    printf("\033[s");

    // Move cursor down and right
    printf("\033[999;999H");

    // Get the cursor position
    get_cursor_pos(r, c);

    // Restore cursor pos
    printf("\033[u");
}

// GO SPEEDY BOY GO !
static inline void plot_char(int x, int y, char k)
{
    printf("\033[%d;%dH\033[33m%c\033[0m", y, x, 'k');
}

static inline void draw_line_low(int x0, int y0, int x1, int y1, char c)
{
    int x = x0;
    int y = y0;
    int dx = x1 - x0;
    int dy = y1 - y0;
    int yi = (dy > 0) ? 1 : -1;
    dy = abs(dy);
    int d = 2 * dy - dx;

    int two_dy = 2 * dy;

    for (; x < x1; x++)
    {
        plot_char(x, y, c);
        if (d > 0)
        {
            y += yi;
            d -= 2 * dx;
        }
        d += two_dy;
    }
}

static inline void draw_line_high(int x0, int y0, int x1, int y1, char c)
{
    int x = x0;
    int y = y0;
    int dx = x1 - x0;
    int dy = y1 - y0;
    int xi = 1;
    if (dx < 0)
    {
        xi = -1;
        dx = -dx;
    }
    int d = 2 * dx - dy;
    for (; y < y1; y++)
    {
        plot_char(x, y, c);
        if (d > 0)
        {
            x += xi;
            d -= 2 * dy;
        }
        d += 2 * dx;
    }
}

void draw_line(int x0, int y0, int x1, int y1, char c)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    if (dy * dy < dx * dx) // we turned ABS off :)
    {
        if (x0 > x1)
            draw_line_low(x1, y1, x0, y0, c);
        else
            draw_line_low(x0, y0, x1, y1, c);
    }
    else
    {
        if (y0 > y1)
            draw_line_high(x1, y1, x0, y0, c);
        else
            draw_line_high(x0, y0, x1, y1, c);
    }
}

static inline void draw(size_t edge_count, mat_t *vertices, edge_t *edges)
{
    float a[2];
    float b[2];
    // Unrolled this loop, again I do not think at this stage
    // it really matters in the grand scheme of things.
    for (size_t i = 0; i < edge_count; i += 4)
    {
        size_t ai0 = edges[i].a;
        size_t bi0 = edges[i].b;
        float a0[2] = {vertices->data[ai0], vertices->data[ai0 + vertices->size.columns]};
        float b0[2] = {vertices->data[bi0], vertices->data[bi0 + vertices->size.columns]};
        draw_line(a0[0], a0[1], b0[0], b0[1], 'C');

        size_t ai1 = edges[i + 1].a;
        size_t bi1 = edges[i + 1].b;
        float a1[2] = {vertices->data[ai1], vertices->data[ai1 + vertices->size.columns]};
        float b1[2] = {vertices->data[bi1], vertices->data[bi1 + vertices->size.columns]};
        draw_line(a1[0], a1[1], b1[0], b1[1], 'C');

        size_t ai2 = edges[i + 2].a;
        size_t bi2 = edges[i + 2].b;
        float a2[2] = {vertices->data[ai2], vertices->data[ai2 + vertices->size.columns]};
        float b2[2] = {vertices->data[bi2], vertices->data[bi2 + vertices->size.columns]};
        draw_line(a2[0], a2[1], b2[0], b2[1], 'C');

        size_t ai3 = edges[i + 3].a;
        size_t bi3 = edges[i + 3].b;
        float a3[2] = {vertices->data[ai3], vertices->data[ai3 + vertices->size.columns]};
        float b3[2] = {vertices->data[bi3], vertices->data[bi3 + vertices->size.columns]};
        draw_line(a3[0], a3[1], b3[0], b3[1], 'C');
    }
}

static inline void mat_multiply(mat_t *out, mat_t *a, mat_t *b)
{
    assert(a->size.columns == b->size.rows);
    assert(out->size.rows == a->size.rows);
    assert(out->size.columns == b->size.columns);

    for (size_t r = 0; r < out->size.rows; r++)
    {
        for (size_t c = 0; c < out->size.columns; c++)
        {
            float sum = 0.0;

            for (size_t i = 0; i < a->size.columns; i += 4)
            {
                sum += a->data[r * a->size.columns + i] * b->data[i * b->size.columns + c];
                sum += a->data[r * a->size.columns + i + 1] * b->data[(i + 1) * b->size.columns + c];
                sum += a->data[r * a->size.columns + i + 2] * b->data[(i + 2) * b->size.columns + c];
                sum += a->data[r * a->size.columns + i + 3] * b->data[(i + 3) * b->size.columns + c];
            }

            out->data[r * out->size.columns + c] = sum;
        }
    }
}

// Rotation matrix of angle r about axis <xyz> -> m
// Thu Mar 30 07:41:27 AM EEST 2023:
// Now using some variable magic to avoid redundant calculations - tho
// at this stage I do not think it matters much.
static inline void mat_rotation(mat_t *m, float x, float y, float z, float r)
{
    const float *out = m->data;

    const float c = cosf(r);
    const float oc = 1 - c;
    const float s = sinf(r);

    const float xxoc = x * x * oc;
    const float yyoc = y * y * oc;
    const float zzoc = z * z * oc;

    m->data[0] = c + xxoc;
    m->data[1] = x * y * oc - z * s;
    m->data[2] = x * z * oc + y * s;
    m->data[3] = 0;

    m->data[4] = y * x * oc + z * s;
    m->data[5] = c + yyoc;
    m->data[6] = y * z * oc - x * s;
    m->data[7] = 0;

    m->data[8] = z * x * oc - y * s;
    m->data[9] = z * y * oc + x * s;
    m->data[10] = c + zzoc;
    m->data[11] = 0;

    m->data[12] = 0;
    m->data[13] = 0;
    m->data[14] = 0;
    m->data[15] = 1;
}

static inline void mat_copy(mat_t *dest, mat_t *source)
{
    assert(dest->size.rows == source->size.rows);
    assert(dest->size.columns == source->size.columns);

    size_t total = dest->size.rows * dest->size.columns;
    if (total == 16 && ((uintptr_t)dest->data % 32 == 0) && ((uintptr_t)source->data % 32 == 0))
    {
        // changed to avx2 beacause why doesnt have a avx compatible cpu in 2025?
        __m256 a = _mm256_load_ps(source->data);
        __m256 b = _mm256_load_ps(source->data + 8);
        _mm256_store_ps(dest->data, a);
        _mm256_store_ps(dest->data + 8, b);
    }
    else
    {
        if (dest->size.rows == source->size.rows && dest->size.columns == source->size.columns)
        {
            memcpy(dest->data, source->data, total * sizeof(float));
        }
        else
        {
            size_t bytes = dest->size.rows * dest->size.columns * sizeof(float);
            memmove(dest->data, source->data, bytes);
        }
    }
}

void mat_print(mat_t *m)
{
    for (size_t r = 0; r < m->size.rows; r++)
    {
        for (size_t c = 0; c < m->size.columns; c++)
        {
            printf("%g ", m->data[r * m->size.columns + c]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    int c;
    int frametime = DEFAULT_FRAMETIME;

    // Flags
    static struct option long_options[] = {
        {"fps", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    while ((c = getopt_long(argc, argv, "f:vh", long_options, NULL)) != -1)
    {
        switch (c)
        {
        case 'f':
            frametime = atoi(optarg);
            break;
        case '?':
            usleep(2000000);
            break;
        case 'h':
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  --f, --fps N    Set frametime to N [Default: 11111.11]\n");
            printf("  --h, --help     Show this help message and exit\n");
            exit(0);

        default:
            abort();
        }
    }
    // Current time of the animation
    int time = 0;

    // Space to hold the transformed positions of the vertices.
    float output_data[32];
    mat_t output = {
        .size = {
            .rows = 4,
            .columns = 8},
        .data = output_data};

    // Matrix of column vectors representing the vertices of a cube with side
    // length 2 and centered on 0,0,0...
    float vertex_data[] = {
        -1, 1, -1, 1, -1, 1, -1, 1,
        -1, -1, 1, 1, -1, -1, 1, 1,
        -1, -1, -1, -1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1};
    mat_t vertices = {
        .size = {
            .rows = 4,
            .columns = 8},
        .data = vertex_data};

    // Tuples of vertex indices which are connected by edges
    edge_t edges[12] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7}, {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    // Get the terminal size to determine how to scale the cube.
    size_t trows;
    size_t tcols;

    get_terminal_size(&trows, &tcols);

    float size;
    if (trows < tcols)
        size = (float)trows / 6;
    else
        size = (float)tcols / 6;

    // Transformation to move and scale the cube to fit the window.
    float view_data[] = {size, 0, 0, tcols / 2,
                         0, size, 0, trows / 2,
                         0, 0, size, 5,
                         0, 0, 0, 1};
    mat_t view_matrix = {
        .size = {4, 4},
        .data = view_data};

    // Space for the final transform of the cube.
    float transform_data[16];
    mat_t transform_matrix = {
        .size = {4, 4},
        .data = transform_data};

    // Populated with various rotation matrices and multiplied into the
    // final transform.
    float rotation_data[16];
    mat_t rotation_matrix = {
        .size = {4, 4},
        .data = rotation_data};

    // The destination for matrix multiplication cannot be one of the
    // coefficients... this matrix is used to repeatedly multiply the
    // final transform by rotation matrices.
    float working_data[16];
    mat_t working_matrix = {
        .size = {4, 4},
        .data = working_data};

    int loop_count = 0;
    while (loop_count < LOOP_LIMIT || LOOP_LIMIT == -1)
    {
        clear_screen();
        time++;

        mat_rotation(&rotation_matrix, 0, 1, 0, (float)time * 0.01);
        mat_multiply(&transform_matrix, &view_matrix, &rotation_matrix);

        mat_copy(&working_matrix, &transform_matrix);
        mat_rotation(&rotation_matrix, 1, 0, 0, (float)time * 0.033);
        mat_multiply(&transform_matrix, &working_matrix, &rotation_matrix);

        mat_copy(&working_matrix, &transform_matrix);
        mat_rotation(&rotation_matrix, 0, 0, 1, (float)time * 0.021);
        mat_multiply(&transform_matrix, &working_matrix, &rotation_matrix);

        mat_multiply(&output, &transform_matrix, &vertices);

        draw(12, &output, edges);
        fflush(stdout);

        if (usleep(frametime))
            break;
        loop_count++;
        if (loop_count == LOOP_LIMIT && LOOP_LIMIT != -1)
        {
            return 0;
        }
    }

    return 0;
}
