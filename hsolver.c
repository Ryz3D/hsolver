#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <math.h>

// TODO:
//  - commands for char settings (sep_char_in/_out, dec_sep_char_in/_out)
//  - console colors/bold (especially "list")

#define HS_MAX_FRAC_DIGITS 13
#define HS_BUF_SIZE 64
#define HS_EPSILON 1e-20
#define HS_MAX_EXP_LIST_LEN 30
#define HS_FORCE_INTERACTIVE 0

#ifdef WIN
#define ENDL "\r\n"
#define SIZE_T_F "%i"
#endif
#ifdef UNIX
#define ENDL "\n"
#define SIZE_T_F "%lu"
#endif

#ifndef ENDL
#warning "Please specify platform using -D WIN or -D UNIX"
#define ENDL "\n"
#endif
#ifndef SIZE_T_F
#define SIZE_T_F "%lu"
#endif

#define HS_ZERO ((hs_value_t){.re = 0, .im = 0})
#define HS_ONE ((hs_value_t){.re = 1, .im = 0})

const char *help_text = \
"--COMMANDS--" ENDL \
"  help" ENDL \
"  list" ENDL \
"  settings" ENDL \
"  bin [optional inline expression]" ENDL \
"  oct [optional inline expression]" ENDL \
"  dec [optional inline expression]" ENDL \
"  hex [optional inline expression]" ENDL \
"  scient_min = expression" ENDL \
"  scient_max = expression" ENDL \
"  sep_out = expression" ENDL \
;

typedef struct hs_value {
    double re;
    double im;
} hs_value_t;

typedef struct hs_var {
    char id[HS_BUF_SIZE];
    hs_value_t value;
} hs_var_t;

hs_var_t hs_default_vars[] = {
    {.id = "zero",   .value = {.re = 0,                .im = 0}},
    {.id = "one",    .value = {.re = 1,                .im = 0}},
    {.id = "inch",   .value = {.re = 0.0254,           .im = 0}},
    {.id = "foot",   .value = {.re = 0.3048,           .im = 0}},
    {.id = "mile",   .value = {.re = 1609.34,          .im = 0}},
    {.id = "i",      .value = {.re = 0,                .im = 1}},
    {.id = "e",      .value = {.re = 2.718281828459045235360287471352662497757247093, .im = 0}},
    {.id = "pi",     .value = {.re = 3.141592653589793238462643383279502884197169399, .im = 0}},
    {.id = "tau",    .value = {.re = 6.283185307179586476925286766559005768394338798, .im = 0}},
    {.id = "phi",    .value = {.re = 1.618033988749894848204586834365638117720309179, .im = 0}},
    {.id = "c",      .value = {.re = 299792458,        .im = 0}},
    {.id = "g",      .value = {.re = 9.80665,          .im = 0}},
    {.id = "ev",     .value = {.re = 1.602176634e-19,  .im = 0}},
    {.id = "mu_0",   .value = {.re = 1.25663706127e-6, .im = 0}},
    {.id = "epsi_0", .value = {.re = 8.8541878188e-12, .im = 0}},
};

hs_value_t hs_f_abs(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = sqrt(a.re * a.re + a.im * a.im), .im = 0};
}

hs_value_t hs_f_add(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = a.re + b.re, .im = a.im + b.im};
}

hs_value_t hs_f_subtract(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = a.re - b.re, .im = a.im - b.im};
}

hs_value_t hs_f_multiply(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = a.re * b.re - a.im * b.im, .im = a.re * b.im + a.im * b.re};
}

hs_value_t hs_f_divide(hs_value_t a, hs_value_t b) {
    if (hs_f_abs(b, HS_ZERO).re < HS_EPSILON) {
        printf("ERROR: division by zero" ENDL);
        return (hs_value_t){.re = NAN, .im = NAN};
    }
    return (hs_value_t){.re = (a.re * b.re + a.im * b.im) / (b.re * b.re + b.im * b.im), .im = (a.im * b.re - a.re * b.im) / (b.re * b.re + b.im * b.im)};
}

hs_value_t hs_f_round(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = round(a.re), .im = round(a.im)};
}

hs_value_t hs_f_floor(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = floor(a.re), .im = floor(a.im)};
}

hs_value_t hs_f_ceil(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = ceil(a.re), .im = ceil(a.im)};
}

hs_value_t hs_f_modulo(hs_value_t a, hs_value_t b) {
    if (fabs(b.im) < HS_EPSILON) {
        return (hs_value_t){.re = fmod(a.re, b.re), .im = 0};
    } else {
        return (hs_value_t){.re = fmod(a.re, b.re), .im = fmod(a.im, b.im)};
    }
}

hs_value_t hs_f_pow(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON || fabs(b.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> exponent) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = pow(a.re, b.re), .im = 0};
}

hs_value_t hs_f_root(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON || fabs(b.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> root) with complex numbers" ENDL);
    }
    return hs_f_pow(a, hs_f_divide(HS_ONE, b));
}

hs_value_t hs_f_sqrt(hs_value_t a, hs_value_t b) {
    if (fabs(b.im) >= HS_EPSILON) {
        return (hs_value_t){.re = sqrt((hs_f_abs(a, HS_ZERO).re + a.re) / 2), .im = a.im / fabs(a.im) * sqrt((hs_f_abs(a, HS_ZERO).re - a.re) / 2)};
    } else if (hs_f_abs(hs_f_subtract(a, (hs_value_t){.re = -1, .im = 0}), HS_ZERO).re < HS_EPSILON) {
        return (hs_value_t){.re = 0, .im = 1};
    }
    return hs_f_root(a, (hs_value_t){.re = 2, .im = 0});
}

hs_value_t hs_f_ln(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = log(sqrt(a.re * a.re + a.im * a.im)), .im = atan2(a.im, a.re)};
}

hs_value_t hs_f_log2(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> log2) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = log2(a.re), .im = 0};
}

hs_value_t hs_f_log10(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> log10) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = log10(a.re), .im = 0};
}

hs_value_t hs_f_sin(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> sin) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = sin(a.re), .im = 0};
}

hs_value_t hs_f_sinh(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> sinh) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = sinh(a.re), .im = 0};
}

hs_value_t hs_f_asin(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> asin) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = asin(a.re), .im = 0};
}

hs_value_t hs_f_cos(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> cos) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = cos(a.re), .im = 0};
}

hs_value_t hs_f_cosh(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> cosh) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = cosh(a.re), .im = 0};
}

hs_value_t hs_f_acos(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> acos) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = acos(a.re), .im = 0};
}

hs_value_t hs_f_tan(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> tan) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = tan(a.re), .im = 0};
}

hs_value_t hs_f_tanh(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> tanh) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = tanh(a.re), .im = 0};
}

hs_value_t hs_f_atan(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> atan) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = atan(a.re), .im = 0};
}

hs_value_t hs_f_atan2(hs_value_t a, hs_value_t b) {
    if (fabs(a.im) >= HS_EPSILON || fabs(b.im) >= HS_EPSILON) {
        printf("ERROR: i'm sorry dave, i can't let you do that (-> atan2) with complex numbers" ENDL);
    }
    return (hs_value_t){.re = atan2(a.re, b.re), .im = 0};
}

hs_value_t hs_f_and(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = (uint64_t)a.re & (uint64_t)b.re, .im = (uint64_t)a.im & (uint64_t)b.im};
}

hs_value_t hs_f_or(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = (uint64_t)a.re | (uint64_t)b.re, .im = (uint64_t)a.im | (uint64_t)b.im};
}

hs_value_t hs_f_xor(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = (uint64_t)a.re ^ (uint64_t)b.re, .im = (uint64_t)a.im ^ (uint64_t)b.im};
}

hs_value_t hs_f_shiftl(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = (uint64_t)a.re << (uint64_t)b.re, .im = (uint64_t)a.im << (uint64_t)b.im};
}

hs_value_t hs_f_shiftr(hs_value_t a, hs_value_t b) {
    return (hs_value_t){.re = (uint64_t)a.re >> (uint64_t)b.re, .im = (uint64_t)a.im >> (uint64_t)b.im};
}

typedef struct hs_func_param hs_func_param_t;

typedef struct hs_func {
    char id[HS_BUF_SIZE];
    hs_value_t (*func)(hs_value_t a, hs_value_t b);
    uint8_t params_count;
    hs_func_param_t *params_linked;
    char *expression;
} hs_func_t;

typedef struct hs_func_param {
    char id[HS_BUF_SIZE];
    hs_func_param_t *next;
} hs_func_param_t;

hs_func_t hs_default_funcs[] = {
    {.id = "add",       .func = hs_f_add,       .params_count = 2},
    {.id = "subtract",  .func = hs_f_subtract,  .params_count = 2},
    {.id = "multiply",  .func = hs_f_multiply,  .params_count = 2},
    {.id = "divide",    .func = hs_f_divide,    .params_count = 2},
    {.id = "modulo",    .func = hs_f_modulo,    .params_count = 2},
    {.id = "pow",       .func = hs_f_pow,       .params_count = 2},
    {.id = "root",      .func = hs_f_root,      .params_count = 2},
    {.id = "sqrt",      .func = hs_f_sqrt,      .params_count = 1},
    {.id = "round",     .func = hs_f_round,     .params_count = 1},
    {.id = "floor",     .func = hs_f_floor,     .params_count = 1},
    {.id = "ceil",      .func = hs_f_ceil,      .params_count = 1},
    {.id = "abs",       .func = hs_f_abs,       .params_count = 1},
    {.id = "ln",        .func = hs_f_ln,        .params_count = 1},
    {.id = "log2",      .func = hs_f_log2,      .params_count = 1},
    {.id = "log10",     .func = hs_f_log10,     .params_count = 1},
    {.id = "sin",       .func = hs_f_sin,       .params_count = 1},
    {.id = "sinh",      .func = hs_f_sinh,      .params_count = 1},
    {.id = "asin",      .func = hs_f_asin,      .params_count = 1},
    {.id = "cos",       .func = hs_f_cos,       .params_count = 1},
    {.id = "cosh",      .func = hs_f_cosh,      .params_count = 1},
    {.id = "acos",      .func = hs_f_acos,      .params_count = 1},
    {.id = "tan",       .func = hs_f_tan,       .params_count = 1},
    {.id = "tanh",      .func = hs_f_tanh,      .params_count = 1},
    {.id = "atan",      .func = hs_f_atan,      .params_count = 1},
    {.id = "atan2",     .func = hs_f_atan2,     .params_count = 1},
    {.id = "and",       .func = hs_f_and,       .params_count = 2},
    {.id = "or",        .func = hs_f_or,        .params_count = 2},
    {.id = "xor",       .func = hs_f_xor,       .params_count = 2},
    {.id = "shiftl",    .func = hs_f_shiftl,    .params_count = 2},
    {.id = "shiftr",    .func = hs_f_shiftr,    .params_count = 2},
};

typedef enum hs_output_mode {
    HS_OUTPUT_BIN = 2,
    HS_OUTPUT_OCT = 8,
    HS_OUTPUT_DEC = 10,
    HS_OUTPUT_HEX = 16,
} hs_output_mode_t;

typedef struct hs_settings {
    hs_output_mode_t output_mode;
    double scient_min;
    double scient_max;
    char dec_sep_char_in;
    char dec_sep_char_out;
    bool sep_out;
    char sep_char_in;
    char sep_char_out;
} hs_settings_t;

typedef struct hs_state {
    hs_var_t *context_vars;
    size_t context_vars_length;
    hs_func_t *context_funcs;
    size_t context_funcs_length;
    hs_settings_t settings;
} hs_state_t;

bool hs_funcs_push(hs_state_t *state, hs_func_t func);

hs_state_t hs_default_state() {
    hs_state_t state = {
        .context_vars = malloc(sizeof(hs_default_vars) + 1 * sizeof(hs_var_t)),
        .context_vars_length = sizeof(hs_default_vars) / sizeof(hs_var_t) + 1,
        .context_funcs = malloc(sizeof(hs_default_funcs)),
        .context_funcs_length = sizeof(hs_default_funcs) / sizeof(hs_func_t),
        .settings = {
            .output_mode = HS_OUTPUT_DEC,
            .scient_min = 0.01,
            .scient_max = 10000,
            .dec_sep_char_in = '.',
            .dec_sep_char_out = '.',
            .sep_out = true,
            .sep_char_in = '\'',
            .sep_char_out = '\'',
        },
    };
    if (state.context_vars == NULL) {
        printf("ERROR: out of memory during variable list initialization :(" ENDL);
        return state;
    }
    if (state.context_funcs == NULL ) {
        printf("ERROR: out of memory during function list initialization :(" ENDL);
        return state;
    }
    state.context_vars[0] = (hs_var_t){
        .id = "ans",
        .value = HS_ZERO,
    };
    for (size_t i = 0; i < sizeof(hs_default_vars) / sizeof(hs_var_t); i++) {
        state.context_vars[i + 1] = hs_default_vars[i];
    }
    for (size_t i = 0; i < state.context_funcs_length; i++) {
        state.context_funcs[i] = hs_default_funcs[i];
        state.context_funcs[i].params_linked = NULL;
        state.context_funcs[i].expression = NULL;
    }

    return state;
}

bool hs_str_same(char*, char*);

bool hs_vars_push(hs_state_t *state, hs_var_t var) {
    size_t var_i = -1;
    for (size_t i = 0; i < state->context_vars_length; i++) {
        if (hs_str_same(var.id, state->context_vars[i].id)) {
            var_i = i;
            break;
        }
    }
    if (var_i == -1) {
        state->context_vars_length++;
        state->context_vars = realloc(state->context_vars, state->context_vars_length * sizeof(hs_var_t));
        if (state->context_vars == NULL) {
            printf("ERROR: out of memory during variable list reallocation at " SIZE_T_F " tokens :(" ENDL, state->context_vars_length);
            return false;
        }
        var_i = state->context_vars_length - 1;
    }
    state->context_vars[var_i] = var;
    return true;
}

void hs_param_free_recursive(hs_func_param_t *param) {
    if (param->next != NULL) {
        hs_param_free_recursive(param->next);
        param->next = NULL;
    }
    if (param != NULL) {
        free(param);
    }
}

bool hs_funcs_push(hs_state_t *state, hs_func_t func) {
    size_t func_i = -1;
    for (size_t i = 0; i < state->context_funcs_length; i++) {
        if (hs_str_same(func.id, state->context_funcs[i].id)) {
            func_i = i;
            if (state->context_funcs[i].expression != NULL)
                free(state->context_funcs[i].expression);
            if (state->context_funcs[i].params_linked != NULL)
                hs_param_free_recursive(state->context_funcs[i].params_linked);
            break;
        }
    }
    if (func_i == -1) {
        state->context_funcs_length++;
        state->context_funcs = realloc(state->context_funcs, state->context_funcs_length * sizeof(hs_func_t));
        if (state->context_funcs == NULL) {
            printf("ERROR: out of memory during function list reallocation at " SIZE_T_F " tokens :(" ENDL, state->context_funcs_length);
            return false;
        }
        func_i = state->context_funcs_length - 1;
    }
    state->context_funcs[func_i] = func;
    return true;
}

void hs_preprocess_input(char *input) {
    while (*input != '\0') {
        if (*input >= 'A' && *input <= 'Z') {
            *input += 'a' - 'A';
        }
        input++;
    }
}

typedef enum hs_token_kind {
    HS_TOKEN_EOF,
    HS_TOKEN_ID,
    HS_TOKEN_ID_IS_VAR,
    HS_TOKEN_LIT_DEC,
    HS_TOKEN_LIT_BIN,
    HS_TOKEN_LIT_OCT,
    HS_TOKEN_LIT_HEX,
    HS_TOKEN_ADD,
    HS_TOKEN_SUBTRACT,
    HS_TOKEN_MULTIPLY,
    HS_TOKEN_DIVIDE,
    HS_TOKEN_MODULO,
    HS_TOKEN_POWER,
    HS_TOKEN_AND,
    HS_TOKEN_OR,
    HS_TOKEN_XOR,
    HS_TOKEN_SHIFTL,
    HS_TOKEN_SHIFTR,
    HS_TOKEN_OPEN_P,
    HS_TOKEN_CLOSE_P,
    HS_TOKEN_COMMA,
} hs_token_kind_t;

typedef struct hs_token {
    hs_token_kind_t kind;
    char content[HS_BUF_SIZE];
} hs_token_t;

typedef struct hs_token_list {
    hs_token_t *items;
    size_t capacity;
    size_t size;
} hs_token_list_t;

hs_token_list_t hs_token_list_init() {
    hs_token_list_t list = {
        .items = malloc(sizeof(hs_token_t)),
        .capacity = 1,
        .size = 0,
    };
    if (list.items == NULL) {
        printf("ERROR: out of memory during token list initialization :(" ENDL);
        return list;
    }
    return list;
}

bool hs_token_list_push(hs_token_list_t *list, hs_token_t token) {
    if (list->size >= list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, list->capacity * sizeof(hs_token_t));
        if (list->items == NULL) {
            printf("ERROR: out of memory during token list reallocation at " SIZE_T_F " tokens :(" ENDL, list->size);
            return false;
        }
    }
    list->items[list->size++] = token;
    return true;
}

hs_token_t hs_token_list_pop(hs_token_list_t *list) {
    if (list->size > 0) {
        list->size--;
        return list->items[list->size];
    } else {
        printf("WARNING: missing some expected token" ENDL);
        return (hs_token_t){
            .kind = HS_TOKEN_EOF,
        };
    }
}

hs_token_list_t hs_tokenize(char *input, hs_state_t *state) {
    hs_token_list_t tokens = hs_token_list_init();

    if (tokens.items == NULL)
        goto hs_tokenize_error;

    for (size_t i = 0; input[i] != '\0'; i++) {
        if (input[i] == '+') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_ADD})) goto hs_tokenize_error;
        } else if (input[i] == '-') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_SUBTRACT})) goto hs_tokenize_error;
        } else if (input[i] == '*') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_MULTIPLY})) goto hs_tokenize_error;
        } else if (input[i] == '/') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_DIVIDE})) goto hs_tokenize_error;
        } else if (input[i] == '%') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_MODULO})) goto hs_tokenize_error;
        } else if (input[i] == '^') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_POWER})) goto hs_tokenize_error;
        } else if (input[i] == '&') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_AND})) goto hs_tokenize_error;
        } else if (input[i] == '|') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_OR})) goto hs_tokenize_error;
        } else if (input[i] == '~') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_XOR})) goto hs_tokenize_error;
        } else if (input[i] == '<') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_SHIFTL})) goto hs_tokenize_error;
        } else if (input[i] == '>') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_SHIFTR})) goto hs_tokenize_error;
        } else if (input[i] == '(') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_OPEN_P})) goto hs_tokenize_error;
        } else if (input[i] == ')') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_CLOSE_P})) goto hs_tokenize_error;
        } else if (input[i] == ',') {
            if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_COMMA})) goto hs_tokenize_error;
        } else if ((input[i] >= '0' && input[i] <= '9') || input[i] == state->settings.dec_sep_char_in) {
            hs_token_t token_lit;
            if (input[i] == '0') {
                if (input[i + 1] == 'b') {
                    i += 2;
                    size_t token_start = i;
                    token_lit.kind = HS_TOKEN_LIT_BIN;
                    while (((input[i] >= '0' && input[i] <= '1') || input[i] == state->settings.dec_sep_char_in || input[i] == state->settings.sep_char_in) && i - token_start < HS_BUF_SIZE - 1) {
                        token_lit.content[i - token_start] = input[i];
                        i++;
                    }
                    token_lit.content[i - token_start] = '\0';
                    i--;
                    if (!hs_token_list_push(&tokens, token_lit))
                        goto hs_tokenize_error;
                    continue;
                } else if (input[i + 1] == 'o') {
                    i += 2;
                    size_t token_start = i;
                    token_lit.kind = HS_TOKEN_LIT_OCT;
                    while (((input[i] >= '0' && input[i] <= '7') || input[i] == state->settings.dec_sep_char_in || input[i] == state->settings.sep_char_in) && i - token_start < HS_BUF_SIZE - 1) {
                        token_lit.content[i - token_start] = input[i];
                        i++;
                    }
                    token_lit.content[i - token_start] = '\0';
                    i--;
                    if (!hs_token_list_push(&tokens, token_lit))
                        goto hs_tokenize_error;
                    continue;
                } else if (input[i + 1] == 'x') {
                    i += 2;
                    size_t token_start = i;
                    token_lit.kind = HS_TOKEN_LIT_HEX;
                    while (((input[i] >= '0' && input[i] <= '9') || (input[i] >= 'a' && input[i] <= 'f') || input[i] == state->settings.dec_sep_char_in || input[i] == state->settings.sep_char_in) && i - token_start < HS_BUF_SIZE - 1) {
                        token_lit.content[i - token_start] = input[i];
                        i++;
                    }
                    token_lit.content[i - token_start] = '\0';
                    i--;
                    if (!hs_token_list_push(&tokens, token_lit))
                        goto hs_tokenize_error;
                    continue;
                }
            }
            size_t token_start = i;
            token_lit.kind = HS_TOKEN_LIT_DEC;
            while (((input[i] >= '0' && input[i] <= '9') || input[i] == state->settings.dec_sep_char_in || input[i] == state->settings.sep_char_in) && i - token_start < HS_BUF_SIZE - 1) {
                token_lit.content[i - token_start] = input[i];
                i++;
            }
            token_lit.content[i - token_start] = '\0';
            i--;
            if (tokens.items[tokens.size - 1].kind == HS_TOKEN_SUBTRACT &&
               ((tokens.size >= 2 && (tokens.items[tokens.size - 2].kind == HS_TOKEN_OPEN_P ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_COMMA ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_ADD ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_SUBTRACT ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_MULTIPLY ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_DIVIDE ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_MODULO ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_POWER ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_AND ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_OR ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_XOR ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_SHIFTL ||
                                     tokens.items[tokens.size - 2].kind == HS_TOKEN_SHIFTR)) || tokens.size == 1)) {
                hs_token_list_pop(&tokens);
                char last_char = '-';
                for (size_t j = 0; last_char != '\0'; j++) {
                    char temp = token_lit.content[j];
                    token_lit.content[j] = last_char;
                    last_char = temp;
                }
            }
            if (!hs_token_list_push(&tokens, token_lit))
                goto hs_tokenize_error;
        } else if (input[i] >= 'a' && input[i] <= 'z') {
            hs_token_t token_id = {.kind = HS_TOKEN_ID};
            size_t token_start = i;
            while (((input[i] >= '0' && input[i] <= '9') || (input[i] >= 'a' && input[i] <= 'z') || input[i] == '_') && i - token_start < HS_BUF_SIZE - 1) {
                token_id.content[i - token_start] = input[i];
                i++;
            }
            token_id.content[i - token_start] = '\0';
            i--;
            if (!hs_token_list_push(&tokens, token_id))
                goto hs_tokenize_error;
        }
    }
    if (!hs_token_list_push(&tokens, (hs_token_t){.kind = HS_TOKEN_EOF}))
        goto hs_tokenize_error;

    return tokens;

hs_tokenize_error:
    return tokens;
}

bool hs_is_op(hs_token_kind_t kind) {
    switch (kind) {
        case HS_TOKEN_ADD:
        case HS_TOKEN_SUBTRACT:
        case HS_TOKEN_MULTIPLY:
        case HS_TOKEN_DIVIDE:
        case HS_TOKEN_MODULO:
        case HS_TOKEN_POWER:
        case HS_TOKEN_AND:
        case HS_TOKEN_OR:
        case HS_TOKEN_XOR:
        case HS_TOKEN_SHIFTL:
        case HS_TOKEN_SHIFTR:
            return true;
        default:
            return false;
    }
}

int8_t hs_op_prio(hs_token_kind_t kind) {
    switch (kind) {
        case HS_TOKEN_OR:
            return 0;
        case HS_TOKEN_XOR:
            return 1;
        case HS_TOKEN_AND:
            return 2;
        case HS_TOKEN_SHIFTL:
        case HS_TOKEN_SHIFTR:
            return 3;
        case HS_TOKEN_ADD:
        case HS_TOKEN_SUBTRACT:
            return 4;
        case HS_TOKEN_MULTIPLY:
        case HS_TOKEN_DIVIDE:
        case HS_TOKEN_MODULO:
            return 5;
        case HS_TOKEN_POWER:
            return 6;
        default:
            return -1;
    }
}

hs_token_list_t hs_shunting_yard(hs_token_list_t tokens) {
    size_t input_i = 0;
    hs_token_list_t output = hs_token_list_init();
    hs_token_list_t stack = hs_token_list_init();

    if (output.items == NULL)
        goto hs_shunting_yard_error;
    if (stack.items == NULL)
        goto hs_shunting_yard_error;

    int32_t last_value = -2;
    while (input_i < tokens.size) {
        switch (tokens.items[input_i].kind) {
            case HS_TOKEN_LIT_DEC:
            case HS_TOKEN_LIT_BIN:
            case HS_TOKEN_LIT_OCT:
            case HS_TOKEN_LIT_HEX:
            case HS_TOKEN_ID:
                if (tokens.items[input_i].kind == HS_TOKEN_ID) {
                    if (tokens.items[input_i + 1].kind == HS_TOKEN_OPEN_P) {
                        // function call
                        if (!hs_token_list_push(&stack, tokens.items[input_i]))
                            goto hs_shunting_yard_error;
                        break;
                    } else {
                        tokens.items[input_i].kind = HS_TOKEN_ID_IS_VAR;
                        if (last_value == input_i - 1) {
                            // implied multiplication
                            while (stack.size > 0 &&
                                   hs_is_op(stack.items[stack.size - 1].kind) &&
                                   hs_op_prio(HS_TOKEN_MULTIPLY) <= hs_op_prio(stack.items[stack.size - 1].kind)) {
                                // TODO: except for exponent, possibly
                                if (!hs_token_list_push(&output, hs_token_list_pop(&stack)))
                                    goto hs_shunting_yard_error;
                            }
                            if (!hs_token_list_push(&stack, (hs_token_t){.kind = HS_TOKEN_MULTIPLY}))
                                goto hs_shunting_yard_error;
                        }
                    }
                } else {
                    last_value = input_i;
                }
                // var or value
                if (!hs_token_list_push(&output, tokens.items[input_i]))
                    goto hs_shunting_yard_error;
                break;
            case HS_TOKEN_COMMA:
                if (stack.size == 0) {
                    printf("ERROR: unexpected comma" ENDL);
                    goto hs_shunting_yard_error;
                }
                while (stack.items[stack.size - 1].kind != HS_TOKEN_OPEN_P) {
                    if (!hs_token_list_push(&output, hs_token_list_pop(&stack)))
                        goto hs_shunting_yard_error;
                    if (stack.size == 0) {
                        printf("ERROR: unexpected comma" ENDL);
                        goto hs_shunting_yard_error;
                    }
                }
                break;
            case HS_TOKEN_ADD:
            case HS_TOKEN_SUBTRACT:
            case HS_TOKEN_MULTIPLY:
            case HS_TOKEN_DIVIDE:
            case HS_TOKEN_MODULO:
            case HS_TOKEN_POWER:
            case HS_TOKEN_AND:
            case HS_TOKEN_OR:
            case HS_TOKEN_XOR:
            case HS_TOKEN_SHIFTL:
            case HS_TOKEN_SHIFTR:
                while (stack.size > 0 &&
                       hs_is_op(stack.items[stack.size - 1].kind) &&
                       hs_op_prio(tokens.items[input_i].kind) <= hs_op_prio(stack.items[stack.size - 1].kind)) {
                    // TODO: except for exponent, possibly
                    if (!hs_token_list_push(&output, hs_token_list_pop(&stack)))
                        goto hs_shunting_yard_error;
                }
                if (!hs_token_list_push(&stack, tokens.items[input_i]))
                    goto hs_shunting_yard_error;
                break;
            case HS_TOKEN_OPEN_P:
                if (last_value == input_i - 1) {
                    // implied multiplication
                    while (stack.size > 0 &&
                           hs_is_op(stack.items[stack.size - 1].kind) &&
                           hs_op_prio(HS_TOKEN_MULTIPLY) <= hs_op_prio(stack.items[stack.size - 1].kind)) {
                        // TODO: except for exponent, possibly
                        if (!hs_token_list_push(&output, hs_token_list_pop(&stack)))
                            goto hs_shunting_yard_error;
                    }
                    if (!hs_token_list_push(&stack, (hs_token_t){.kind = HS_TOKEN_MULTIPLY}))
                        goto hs_shunting_yard_error;
                }
                if (!hs_token_list_push(&stack, tokens.items[input_i]))
                    goto hs_shunting_yard_error;
                break;
            case HS_TOKEN_CLOSE_P:
                if (stack.size == 0) {
                    printf("ERROR: closing parenthesis without opening one" ENDL);
                    goto hs_shunting_yard_error;
                }
                while (stack.items[stack.size - 1].kind != HS_TOKEN_OPEN_P) {
                    if (stack.size == 0) {
                        printf("ERROR: closing parenthesis without opening one" ENDL);
                        goto hs_shunting_yard_error;
                    }
                    if (!hs_token_list_push(&output, hs_token_list_pop(&stack)))
                        goto hs_shunting_yard_error;
                }
                hs_token_list_pop(&stack);
                if (stack.size > 0 && stack.items[stack.size - 1].kind == HS_TOKEN_ID) {
                    if (!hs_token_list_push(&output, hs_token_list_pop(&stack)))
                        goto hs_shunting_yard_error;
                }
                break;
            case HS_TOKEN_EOF:
            default:
                break;
        }
        input_i++;
    }
    while (stack.size > 0) {
        hs_token_t stack_token = hs_token_list_pop(&stack);
        if (!hs_token_list_push(&output, stack_token))
            goto hs_shunting_yard_error;
    }

    free(stack.items);

    return output;

hs_shunting_yard_error:
    if (stack.items != NULL)
        free(stack.items);

    return output;
}

typedef struct hs_value_list {
    hs_value_t *items;
    size_t capacity;
    size_t size;
} hs_value_list_t;

hs_value_list_t hs_rpn_list_init() {
    hs_value_list_t list = {
        .items = malloc(sizeof(hs_value_t)),
        .capacity = 1,
        .size = 0,
    };
    if (list.items == NULL) {
        printf("ERROR: out of memory during value list initialization :(" ENDL);
        return list;
    }
    return list;
}

bool hs_value_list_push(hs_value_list_t *list, hs_value_t item) {
    if (list->size >= list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, list->capacity * sizeof(hs_value_t));
        if (list->items == NULL) {
            printf("ERROR: out of memory during value list reallocation at " SIZE_T_F " items :(" ENDL, list->size);
            return false;
        }
    }
    if (list->items == NULL) {
        return false;
    }
    list->items[list->size++] = item;
    return true;
}

hs_value_t hs_value_list_pop(hs_value_list_t *list) {
    if (list->size > 0 && list->items != NULL) {
        list->size--;
        return list->items[list->size];
    } else {
        printf("WARNING: missing some expected value" ENDL);
        return HS_ZERO;
    }
}

bool hs_str_same(char *a, char *b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

size_t hs_str_len(char *a) {
    size_t i = 0;
    while (a[i] != '\0')
        i++;
    return i;
}

hs_value_t hs_solve(hs_token_list_t tokens, hs_state_t *state) {
    hs_value_list_t list = hs_rpn_list_init();
    hs_value_t result = HS_ZERO;

    if (list.items == NULL)
        goto hs_solve_error;

    for (size_t i = 0; i < tokens.size; i++) {
        hs_value_t a, b;

        switch (tokens.items[i].kind) {
            case HS_TOKEN_LIT_DEC:
            case HS_TOKEN_LIT_BIN:
            case HS_TOKEN_LIT_OCT:
            case HS_TOKEN_LIT_HEX: {
                hs_value_t lit_value = HS_ZERO;
                int base = 0;
                if (tokens.items[i].kind == HS_TOKEN_LIT_DEC) {
                    base = 10;
                } else if (tokens.items[i].kind == HS_TOKEN_LIT_BIN) {
                    base = 2;
                } else if (tokens.items[i].kind == HS_TOKEN_LIT_OCT) {
                    base = 8;
                } else if (tokens.items[i].kind == HS_TOKEN_LIT_HEX) {
                    base = 16;
                }
                bool frac = false;
                double frac_fac = 1.0 / base;
                bool negative = false;
                size_t j = 0;
                if (tokens.items[i].content[0] == '-') {
                    negative = true;
                    j++;
                }

                for (; j < HS_BUF_SIZE && tokens.items[i].content[j] != '\0'; j++) {
                    if (tokens.items[i].content[j] >= '0' && tokens.items[i].content[j] <= '9') {
                        if (!frac) {
                            lit_value = hs_f_multiply(lit_value, (hs_value_t){.re = base, .im = 0});
                            lit_value = hs_f_add(lit_value, (hs_value_t){.re = tokens.items[i].content[j] - '0', .im = 0});
                        } else {
                            lit_value = hs_f_add(lit_value, (hs_value_t){.re = frac_fac * (double)(tokens.items[i].content[j] - '0'), .im = 0});
                            frac_fac /= (double)base;
                        }
                    } else if (tokens.items[i].content[j] >= 'a' && tokens.items[i].content[j] <= 'z' && base == 16) {
                        if (!frac) {
                            lit_value = hs_f_multiply(lit_value, (hs_value_t){.re = base, .im = 0});
                            lit_value = hs_f_add(lit_value, (hs_value_t){.re = tokens.items[i].content[j] - 'a' + 10, .im = 0});
                        } else {
                            lit_value = hs_f_add(lit_value, (hs_value_t){.re = frac_fac * (double)(tokens.items[i].content[j] - 'a' + 10), .im = 0});
                            frac_fac /= (double)base;
                        }
                    } else if (tokens.items[i].content[j] == state->settings.dec_sep_char_in) {
                        frac = true;
                    } else if (tokens.items[i].content[j] != state->settings.sep_char_in) {
                        printf("WARNING: unexpected token \"%c\" in literal" ENDL, tokens.items[i].content[j]);
                    }
                }
                if (negative)
                    lit_value = (hs_value_t){.re = -lit_value.re, .im = -lit_value.im};
                if (!hs_value_list_push(&list, lit_value))
                    goto hs_solve_error;
                break;
            }
            case HS_TOKEN_ID_IS_VAR: {
                bool var_found = false;
                for (size_t j = 0; j < state->context_vars_length; j++) {
                    if (hs_str_same(tokens.items[i].content, state->context_vars[j].id)) {
                        if (!hs_value_list_push(&list, state->context_vars[j].value))
                            goto hs_solve_error;
                        var_found = true;
                        break;
                    }
                }
                if (!var_found) {
                    printf("ERROR: var %s not found" ENDL, tokens.items[i].content);
                    goto hs_solve_error;
                }
                break;
            }
            case HS_TOKEN_ID: {
                bool function_found = false;
                for (size_t j = 0; j < state->context_funcs_length; j++) {
                    if (hs_str_same(tokens.items[i].content, state->context_funcs[j].id)) {
                        hs_value_t return_value;
                        if (state->context_funcs[j].func == NULL) {
                            hs_state_t call_state = *state;
                            call_state.context_vars = malloc(call_state.context_vars_length * sizeof(hs_var_t));
                            for (size_t k = 0; k < call_state.context_vars_length; k++) {
                                for (size_t l = 0; l < HS_BUF_SIZE; l++) {
                                    call_state.context_vars[k].id[l] = state->context_vars[k].id[l];
                                    if (call_state.context_vars[k].id[l] == '\0')
                                        break;
                                }
                                call_state.context_vars[k].value = state->context_vars[k].value;
                            }

                            for (uint8_t k = 0; k < state->context_funcs[j].params_count; k++) {
                                hs_func_param_t *param = state->context_funcs[j].params_linked;
                                for (uint8_t l = 0; l < state->context_funcs[j].params_count - k - 1; l++) {
                                    if (param->next == NULL) {
                                        printf("ERROR: incorrect number of arguments for %s. expected %hhu" ENDL, state->context_funcs[j].id, state->context_funcs[j].params_count);
                                        goto hs_solve_error;
                                    }
                                    param = param->next;
                                }
                                hs_var_t new_var = {
                                    .value = hs_value_list_pop(&list),
                                };
                                for (size_t l = 0; l < HS_BUF_SIZE; l++) {
                                    new_var.id[l] = param->id[l];
                                    if (new_var.id[l] == '\0')
                                        break;
                                }
                                hs_vars_push(&call_state, new_var);
                            }

                            char *exp_input = malloc(hs_str_len(state->context_funcs[j].expression) + 1);
                            for (size_t k = 0; k < hs_str_len(state->context_funcs[j].expression) + 1; k++) {
                                exp_input[k] = state->context_funcs[j].expression[k];
                            }
                            hs_preprocess_input(exp_input);
                            hs_token_list_t tokens1 = hs_tokenize(exp_input, &call_state);
                            free(exp_input);
                            if (tokens1.items == NULL)
                                goto hs_solve_error;
                            hs_token_list_t tokens2 = hs_shunting_yard(tokens1);
                            free(tokens1.items);
                            if (tokens2.items == NULL)
                                goto hs_solve_error;
                            if (tokens2.size > 0) {
                                return_value = hs_solve(tokens2, &call_state);
                            }
                            free(tokens2.items);

                            free(call_state.context_vars);
                        } else {
                            if (state->context_funcs[j].params_count == 1) {
                                a = hs_value_list_pop(&list);
                                return_value = state->context_funcs[j].func(a, HS_ZERO);
                            } else {
                                b = hs_value_list_pop(&list);
                                a = hs_value_list_pop(&list);
                                return_value = state->context_funcs[j].func(a, b);
                            }
                        }
                        if (!hs_value_list_push(&list, return_value))
                            goto hs_solve_error;
                        function_found = true;
                        break;
                    }
                }
                if (!function_found) {
                    printf("ERROR: function %s not found" ENDL, tokens.items[i].content);
                    goto hs_solve_error;
                }
                break;
            }
            case HS_TOKEN_COMMA:
                printf("ERROR: comma made it to rpn?" ENDL);
                goto hs_solve_error;
            case HS_TOKEN_ADD:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_add(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_SUBTRACT:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_subtract(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_MULTIPLY:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_multiply(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_DIVIDE:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_divide(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_MODULO:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_modulo(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_POWER:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_pow(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_AND:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_and(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_OR:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_or(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_XOR:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_xor(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_SHIFTL:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_shiftl(a, b)))
                    goto hs_solve_error;
                break;
            case HS_TOKEN_SHIFTR:
                b = hs_value_list_pop(&list);
                a = hs_value_list_pop(&list);
                if (!hs_value_list_push(&list, hs_f_shiftr(a, b)))
                    goto hs_solve_error;
                break;
            default:
                break;
        }
    }

    if (list.size == 0) {
        printf("ERROR: something went wrong during rpn calculation" ENDL);
        goto hs_solve_error;
    } else {
        if (list.size > 1) {
            printf("WARNING: multiple entries left at end of rpn, which is slightly odd" ENDL);
        }
        result = hs_value_list_pop(&list);
    }

    free(list.items);

    return result;

hs_solve_error:
    if (list.size > 0 && list.items != NULL)
        result = hs_value_list_pop(&list);
    printf("(possibly erroneous) ");

    free(list.items);

    return result;
}

char hs_1dim_out_buf[64];

void hs_output_1dim_f(double value, hs_state_t *state, int8_t max_digits) {
    uint32_t hs_1dim_i = 0;

    if (max_digits < 0) {
        max_digits = HS_MAX_FRAC_DIGITS;
    }

    if (value <= -HS_EPSILON) {
        putchar('-');
        value = -value;
    }
    double log_base_2 = 1.0;
    uint8_t sep_spacing = 4;
    switch (state->settings.output_mode) {
        case HS_OUTPUT_HEX:
            putchar('0');
            putchar('x');
            log_base_2 = 1.0 / 4.0;
            sep_spacing = 2;
            break;
        case HS_OUTPUT_OCT:
            putchar('0');
            putchar('o');
            log_base_2 = 1.0 / 3.0;
            sep_spacing = 2;
            break;
        case HS_OUTPUT_DEC:
            log_base_2 = 0.301029995664;
            sep_spacing = 3;
            break;
        case HS_OUTPUT_BIN:
            putchar('0');
            putchar('b');
            log_base_2 = 1.0;
            sep_spacing = 4;
            break;
    }
    int32_t highest_digit = (int32_t)(log2(value) * log_base_2) + 1;
    if (highest_digit < 0) {
        highest_digit = 0;
    }
    if (state->settings.output_mode == HS_OUTPUT_BIN) {
        highest_digit = (highest_digit + 3) / 4 * 4 - 1;
    }
    double value_of_digit = pow((int)state->settings.output_mode, highest_digit);
    bool has_trailing = false;
    for (int32_t i = highest_digit; (fabs(value) >= HS_EPSILON && i > -max_digits - 1) || i >= 0; i--) {
        int digit = (int)(value / value_of_digit + 0.001);
        if (digit >= 0 && digit < 10) {
            hs_1dim_out_buf[hs_1dim_i++] = '0' + digit;
        } else if (digit >= 0 && digit < 16) {
            hs_1dim_out_buf[hs_1dim_i++] = 'A' + digit - 10;
        } else {
            break;
        }
        value -= digit * value_of_digit;
        if (i == 0) {
            hs_1dim_out_buf[hs_1dim_i++] = state->settings.dec_sep_char_out;
            has_trailing = true;
        } else if (i % sep_spacing == 0 && i > 0 && state->settings.sep_out) {
            hs_1dim_out_buf[hs_1dim_i++] = state->settings.sep_char_out;
        }
        value_of_digit /= (int)state->settings.output_mode;
    }
    hs_1dim_out_buf[hs_1dim_i] = '\0';

    if (has_trailing) {
        for (hs_1dim_i--; hs_1dim_i > 0; hs_1dim_i--) {
            if (hs_1dim_out_buf[hs_1dim_i] != '0' && hs_1dim_out_buf[hs_1dim_i] != state->settings.sep_char_out) {
                if (hs_1dim_out_buf[hs_1dim_i] == state->settings.dec_sep_char_out) {
                    hs_1dim_i--;
                }
                break;
            }
        }
    }

    size_t trailing_zeros_start = hs_1dim_i;
    bool leading_done = false;
    if (state->settings.output_mode == HS_OUTPUT_BIN) {
        leading_done = true;
    }
    bool output_empty = true;
    for (hs_1dim_i = 0; hs_1dim_i <= trailing_zeros_start; hs_1dim_i++) {
        if ((hs_1dim_out_buf[hs_1dim_i] != '0' && hs_1dim_out_buf[hs_1dim_i] != state->settings.sep_char_out) || leading_done) {
            putchar(hs_1dim_out_buf[hs_1dim_i]);
            output_empty = false;
            leading_done = true;
        }
    }
    if (output_empty)
        putchar('0');
}

void hs_output_1dim(double value, hs_state_t *state) {
    if (fabs(value) < 1e-15) {
        putchar('0');
    } else if ((fabs(value) < state->settings.scient_min || fabs(value) >= state->settings.scient_max) && state->settings.output_mode == HS_OUTPUT_DEC) {
        // scientific output
        int16_t expo = floor(log10(value) / 3.0) * 3;
        hs_output_1dim_f(value / pow(10, expo), state, 3);
        putchar(' ');
        putchar('*');
        putchar(' ');
        putchar('1');
        putchar('0');
        putchar('^');
        hs_output_1dim_f(expo, state, 0);
    } else {
        // normal output
        hs_output_1dim_f(value, state, -1);
    }
}

void hs_output(hs_value_t value, hs_state_t *state) {
    if (fabs(value.im) < HS_EPSILON) {
        hs_output_1dim(value.re, state);
    } else {
        putchar('(');
        hs_output_1dim(value.re, state);
        putchar(' ');
        if (value.im <= -HS_EPSILON) {
            value.im = -value.im;
            putchar('-');
        } else {
            putchar('+');
        }
        putchar(' ');
        hs_output_1dim(value.im, state);
        putchar('i');
        putchar(')');
    }
}

uint8_t hs_handle_commands(hs_token_list_t *tokens1, hs_token_list_t *tokens2, bool *is_number_base, hs_state_t *state) {
    for (size_t i = 0; i < tokens1->size; i++) {
        if (tokens1->items[i].kind == HS_TOKEN_ID) {
            if (hs_str_same(tokens1->items[i].content, "help")) {
                printf(help_text);
                continue;
            } else if (hs_str_same(tokens1->items[i].content, "list")) {
                size_t len_func = 0;
                size_t len_var = 0;
                
                for (size_t j = 0; j < state->context_funcs_length; j++) {
                    size_t len = 3;
                    len += hs_str_len(state->context_funcs[j].id);

                    hs_func_param_t *param = state->context_funcs[j].params_linked;
                    for (uint8_t p = 0; p < state->context_funcs[j].params_count; p++) {
                        if (param == NULL) {
                            len++;
                        } else {
                            len += hs_str_len(param->id);
                            param = param->next;
                        }
                        if (p < state->context_funcs[j].params_count - 1) {
                            len += 2;
                        }
                    }
                    if (state->context_funcs[j].expression != NULL) {
                        size_t exp_len = hs_str_len(state->context_funcs[j].expression);
                        if (exp_len > HS_MAX_EXP_LIST_LEN) {
                            len += HS_MAX_EXP_LIST_LEN + 6;
                        } else {
                            len += exp_len + 3;
                        }
                    }
                    
                    len_func = len > len_func ? len : len_func;
                }
                for (size_t j = 0; j < state->context_vars_length; j++) {
                    size_t len = hs_str_len(state->context_vars[j].id) + 2;
                    len_var = len > len_var ? len : len_var;
                }

                const char *funcs = "FUNCS";
                const char *vars = "VARS";
                printf("--%s", funcs);
                for (size_t s = 0; s <= len_func - (hs_str_len((char *)funcs) + 2); s++) {
                    putchar('-');
                }
                printf("-|--%s", vars);
                for (size_t s = 0; s <= len_var + 2 + HS_MAX_FRAC_DIGITS - hs_str_len((char *)vars); s++) {
                    putchar('-');
                }
                printf(ENDL);

                for (size_t j = 0; j < state->context_vars_length || j < state->context_funcs_length; j++) {
                    if (j < state->context_funcs_length) {
                        size_t len = 3;
                        printf("  %s(", state->context_funcs[j].id);
                        len += hs_str_len(state->context_funcs[j].id);
    
                        hs_func_param_t *param = state->context_funcs[j].params_linked;
                        for (uint8_t p = 0; p < state->context_funcs[j].params_count; p++) {
                            if (param == NULL) {
                                putchar('a' + p);
                                len++;
                            } else {
                                printf("%s", param->id);
                                len += hs_str_len(param->id);
                                param = param->next;
                            }
                            if (p < state->context_funcs[j].params_count - 1) {
                                putchar(',');
                                putchar(' ');
                                len += 2;
                            }
                        }
                        putchar(')');
                        if (state->context_funcs[j].expression != NULL) {
                            putchar(' ');
                            putchar('=');
                            putchar(' ');
                            size_t exp_len = hs_str_len(state->context_funcs[j].expression);
                            if (exp_len > HS_MAX_EXP_LIST_LEN) {
                                for (size_t k = 0; k < HS_MAX_EXP_LIST_LEN; k++) {
                                    putchar(state->context_funcs[j].expression[k]);
                                }
                                printf("...");
                                len += HS_MAX_EXP_LIST_LEN + 6;
                            } else {
                                printf("%s", state->context_funcs[j].expression);
                                len += exp_len + 3;
                            }
                        }
                        for (size_t s = 0; s <= len_func - len; s++) {
                            putchar(' ');
                        }
                    } else {
                        for (size_t s = 0; s <= len_func + 1; s++) {
                            putchar(' ');
                        }
                    }
                    putchar('|');
                    if (j < state->context_vars_length) {
                        printf("  %s", state->context_vars[j].id);
                        for (size_t s = 0; s <= len_var - (hs_str_len(state->context_vars[j].id) + 2); s++) {
                            putchar(' ');
                        }
                        putchar('=');
                        putchar(' ');
                        hs_output(state->context_vars[j].value, state);
                    }
                    printf(ENDL);
                }
                continue;
            } else if (hs_str_same(tokens1->items[i].content, "settings")) {
                printf("--SETTINGS--" ENDL);
                printf("  scient_min = %f" ENDL, state->settings.scient_min);
                printf("  scient_max = %f" ENDL, state->settings.scient_max);
                printf("  dec_sep_char_in = %c" ENDL, state->settings.dec_sep_char_in);
                printf("  dec_sep_char_out = %c" ENDL, state->settings.dec_sep_char_out);
                printf("  sep_out = %i" ENDL, state->settings.sep_out ? 1 : 0);
                printf("  sep_char_in = %c" ENDL, state->settings.sep_char_in);
                printf("  sep_char_out = %c" ENDL, state->settings.sep_char_out);
                continue;
            } else if (hs_str_same(tokens1->items[i].content, "dec")) {
                state->settings.output_mode = HS_OUTPUT_DEC;
                if (is_number_base != NULL)
                    *is_number_base = true;
                continue;
            } else if (hs_str_same(tokens1->items[i].content, "hex")) {
                state->settings.output_mode = HS_OUTPUT_HEX;
                if (is_number_base != NULL)
                    *is_number_base = true;
                continue;
            } else if (hs_str_same(tokens1->items[i].content, "oct")) {
                state->settings.output_mode = HS_OUTPUT_OCT;
                if (is_number_base != NULL)
                    *is_number_base = true;
                continue;
            } else if (hs_str_same(tokens1->items[i].content, "bin")) {
                state->settings.output_mode = HS_OUTPUT_BIN;
                if (is_number_base != NULL)
                    *is_number_base = true;
                continue;
            }
        }
        if (tokens2 != NULL)
            if (!hs_token_list_push(tokens2, tokens1->items[i]))
                return 1;
    }

    return 0;
}

hs_settings_t temp_settings;

void hs_run(char *input, hs_state_t *state) {
    if (state == NULL) {
        return;
    }

    hs_token_list_t tokens1 = {.items = NULL, .size = 0, .capacity = 0};
    hs_token_list_t tokens2 = {.items = NULL, .size = 0, .capacity = 0};
    hs_token_list_t tokens3 = {.items = NULL, .size = 0, .capacity = 0};

    bool restore_settings = false;
    temp_settings = state->settings;

    hs_preprocess_input(input);

    size_t lvalue_i = 0;
    char lvalue_buffer[HS_BUF_SIZE];
    for (; lvalue_i < sizeof(lvalue_buffer); lvalue_i++) {
        if (input[lvalue_i] == '\0') {
            lvalue_i = 0;
            break;
        } else if (input[lvalue_i] == '=') {
            lvalue_buffer[lvalue_i] = '\0';
            break;
        } else {
            lvalue_buffer[lvalue_i] = input[lvalue_i];
        }
    }
    hs_var_t lvalue_var = {
        .id = "",
        .value = HS_ZERO,
    };
    hs_func_t lvalue_func = {
        .id = "",
        .func = NULL,
        .params_count = 0,
        .params_linked = NULL,
        .expression = "",
    };
    if (lvalue_i > 0) {
        hs_token_list_t tokens_lvalue = hs_tokenize(lvalue_buffer, state);
        if (tokens_lvalue.items == NULL)
            goto hs_run_error;
        hs_token_list_t tokens_lvalue2 = hs_token_list_init();
        if (tokens_lvalue2.items == NULL)
            goto hs_run_error;
        if (hs_handle_commands(&tokens_lvalue, &tokens_lvalue2, &restore_settings, state) != 0)
            goto hs_run_error;
        free(tokens_lvalue.items);
        if (tokens_lvalue2.size == 2) {
            if (tokens_lvalue2.items[0].kind == HS_TOKEN_ID) {
                for (size_t i = 0; i < HS_BUF_SIZE; i++) {
                    lvalue_var.id[i] = tokens_lvalue2.items[0].content[i];
                    if (lvalue_var.id[i] == '\0')
                        break;
                }
            } else {
                printf("WARNING: did not understand input left of \"=\", will be ignored" ENDL);
            }
        } else if (tokens_lvalue2.size > 2) {
            if (tokens_lvalue2.items[0].kind == HS_TOKEN_ID &&
                tokens_lvalue2.items[1].kind == HS_TOKEN_OPEN_P) {
                lvalue_func.params_count = 0;
                hs_func_param_t *param = NULL;
                for (size_t t_i = 2; t_i < tokens_lvalue2.size; t_i += 2) {
                    bool is_last = false;
                    if (tokens_lvalue2.items[t_i].kind == HS_TOKEN_CLOSE_P) {
                        break;
                    }
                    if (tokens_lvalue2.items[t_i].kind != HS_TOKEN_ID) {
                        printf("ERROR: invalid format for function definition" ENDL);
                        free(tokens_lvalue2.items);
                        goto hs_run_error;
                    }
                    if (tokens_lvalue2.items[t_i + 1].kind == HS_TOKEN_CLOSE_P) {
                        is_last = true;
                    } else if (tokens_lvalue2.items[t_i + 1].kind != HS_TOKEN_COMMA) {
                        printf("ERROR: invalid format for function definition" ENDL);
                        free(tokens_lvalue2.items);
                        goto hs_run_error;
                    }
                    hs_func_param_t *next_param = malloc(sizeof(hs_func_param_t));
                    if (param != NULL) {
                        param->next = next_param;
                    } else {
                        lvalue_func.params_linked = next_param;
                    }
                    param = next_param;
                    for (size_t s_i = 0; s_i < HS_BUF_SIZE; s_i++) {
                        param->id[s_i] = tokens_lvalue2.items[t_i].content[s_i];
                        if (param->id[s_i] == '\0')
                            break;
                    }
                    param->next = NULL;
                    lvalue_func.params_count++;
                    if (is_last)
                        break;
                }
                for (size_t i = 0; i < HS_BUF_SIZE; i++) {
                    lvalue_func.id[i] = tokens_lvalue2.items[0].content[i];
                    if (lvalue_func.id[i] == '\0')
                        break;
                }
                lvalue_func.expression = malloc(hs_str_len(input + lvalue_i));
                for (size_t i = 0; i < hs_str_len(input + lvalue_i); i++) {
                    lvalue_func.expression[i] = input[lvalue_i + 1 + i];
                }
                hs_funcs_push(state, lvalue_func);
            } else {
                printf("WARNING: did not understand input left of \"=\", will be ignored" ENDL);
            }
        }
        free(tokens_lvalue2.items);
        if (lvalue_func.id[0] != '\0') {
            return;
        }
    }

    tokens1 = hs_tokenize(input + lvalue_i, state);
    if (tokens1.items == NULL)
        goto hs_run_error;

    tokens2 = hs_token_list_init();
    if (tokens2.items == NULL)
        goto hs_run_error;
    if (hs_handle_commands(&tokens1, &tokens2, &restore_settings, state) != 0)
        goto hs_run_error;
    free(tokens1.items);
    if (tokens2.items[0].kind == HS_TOKEN_EOF) {
        restore_settings = false;
    }

    tokens3 = hs_shunting_yard(tokens2);
    if (tokens3.items == NULL)
        goto hs_run_error;
    free(tokens2.items);

    if (tokens3.size > 0) {
        // assuming the first context_var is "ans"
        hs_value_t result = state->context_vars[0].value = hs_solve(tokens3, state);
        if (lvalue_var.id[0] != '\0') {
            if (hs_str_same(lvalue_var.id, "scient_min")) {
                state->settings.scient_min = result.re;
            } else if (hs_str_same(lvalue_var.id, "scient_max")) {
                state->settings.scient_max = result.re;
            } else if (hs_str_same(lvalue_var.id, "sep_out")) {
                state->settings.sep_out = fabs(result.re) >= HS_EPSILON;
            } else {
                lvalue_var.value = result;
                hs_vars_push(state, lvalue_var);
            }
        }
        hs_output(result, state);
        printf(ENDL);
    }
    free(tokens3.items);
    if (restore_settings)
        state->settings = temp_settings;
    return;

hs_run_error:
    if (tokens1.items != NULL)
        free(tokens1.items);
    if (tokens2.items != NULL)
        free(tokens2.items);
    if (tokens3.items != NULL)
        free(tokens3.items);

    if (restore_settings)
        state->settings = temp_settings;
}

int main(int argc, char *argv[]) {
    size_t hs_input_size = 1 * sizeof(char);
    char *hs_input = malloc(hs_input_size);
    hs_input[0] = '\0';
    size_t hs_input_i = 0;

    hs_state_t state = hs_default_state();
    if (state.context_vars == NULL || state.context_funcs == NULL)
        return 1;

#if !HS_FORCE_INTERACTIVE
    if (argc > 1) {
        // use quotes for command line argument
        for (int i = 1; i < argc; i++) {
            for (size_t j = 0; argv[i][j] != '\0'; j++) {
                if (hs_input_i >= hs_input_size - 1) {
                    hs_input_size *= 2;
                    hs_input = realloc(hs_input, hs_input_size);
                    if (hs_input == NULL) {
                        printf("ERROR: out of memory while reading input :(" ENDL);
                        return 1;
                    }
                }
                hs_input[hs_input_i++] = argv[i][j];
            }
        }
        hs_input[hs_input_i] = '\0';

        hs_run(hs_input, &state);
    } else {
#endif
        while (true) {
            printf("> ");
            for (hs_input_i = 0; (hs_input[hs_input_i] = getchar()) != '\n'; hs_input_i++) {
                if (hs_input_i >= hs_input_size - 1) {
                    hs_input_size *= 2;
                    hs_input = realloc(hs_input, hs_input_size);
                    if (hs_input == NULL) {
                        printf("ERROR: out of memory while reading input :(" ENDL);
                        return 1;
                    }
                }
            }
            hs_input[hs_input_i] = '\0';

            if (hs_input_i == 0) {
                break;
            }
            hs_run(hs_input, &state);
        }

        if (state.context_vars != NULL)
            free(state.context_vars);
        if (state.context_funcs != NULL)
            free(state.context_funcs);
#if !HS_FORCE_INTERACTIVE
    }
#endif

    free(hs_input);

    return 0;
}
