# include <string.h>
# include <stdio.h>
# include <errno.h>
# include <malloc.h>
# include "../runtime/runtime.h"

void *__start_custom_data;
void *__stop_custom_data;

/* The unpacked representation of bytecode file */
typedef struct bytefile_t {
    char *string_ptr;              /* A pointer to the beginning of the string table */
    int  *public_ptr;              /* A pointer to the beginning of publics table    */
    char *code_ptr;                /* A pointer to the bytecode itself               */
    int   stringtab_size;          /* The size (in bytes) of the string table        */
    int   global_area_size;        /* The size (in words) of global area             */
    int   public_symbols_number;   /* The number of public symbols                   */
    char  buffer[0];
} bytefile;

/* Gets a string from a string table by an index */
char* get_string (bytefile *f, int pos) {
    return &f->string_ptr[pos];
}

/* Gets a name for a public symbol */
char* get_public_name (bytefile *f, int i) {
    return get_string (f, f->public_ptr[i*2]);
}

/* Gets an offset for a publie symbol */
int get_public_offset (bytefile *f, int i) {
    return f->public_ptr[i*2+1];
}

/* Reads a binary bytecode file by name and unpacks it */
bytefile* read_file (char *fname) {
    FILE *f = fopen (fname, "rb");
    long size;
    bytefile *file;

    if (f == 0) {
        failure ("%s\n", strerror (errno));
    }

    if (fseek (f, 0, SEEK_END) == -1) {
        failure ("%s\n", strerror (errno));
    }

    file = (bytefile*) malloc (sizeof(int)*3 + (size = ftell (f)));

    if (file == 0) {
        failure ("*** FAILURE: unable to allocate memory.\n");
    }

    rewind (f);

    if (size != fread (&file->stringtab_size, 1, size, f)) {
        failure ("%s\n", strerror (errno));
    }

    fclose (f);

    file->string_ptr = &file->buffer [file->public_symbols_number * 2 * sizeof(int)];
    file->public_ptr = (int*) file->buffer;
    file->code_ptr   = &file->string_ptr [file->stringtab_size];

    return file;
}

enum value_type {
    type_int = 0,
    type_string = 1,
    type_sexp = 2,
    type_reference = 3,
    type_closure = 4,
    type_empty = 5,
    type_array = 6,
};

enum designation_type {
    dsgn_global = 0,
    dsgn_local = 1,
    dsgn_arg = 2,
    dsgn_access = 3
};

typedef struct designation {
    enum designation_type type;
    char* name;
    int pos;
} designation;

typedef struct value {
    enum value_type type;
    void* content;
} value;

typedef struct sexp {
    char* name;
    int arity;
    value values[6];
} sexp;

typedef struct closure {
    value* values;
    int value_num;
    int ip;
} closure;

typedef struct array {
    value* values;
    int value_num;
} array;

long long to_int(value v) {
    if (v.type != type_int) {
        // TODO
        return -1;
    }
    return *((long long*) v.content);
}

value int_value(long long* v) {
    return (value){ type_int, v};
}

value string_value(char* v) {
    return (value){ type_string, v};
}

value sexp_value(sexp* sx) {
    return (value){ type_sexp, sx};
}

value ref_value(designation* ds) {
    return (value){ type_reference, ds};
}

value closure_value(closure* cls) {
    return (value){ type_closure, cls};
}

value empty_value() {
    return (value){ type_empty, NULL};
}

value array_value(array* arr) {
    return (value){ type_array, arr};
}

value int_value_of(long long v) {
    long long* value = malloc(sizeof(long long));
    *value = v;
    return int_value(value);
}

typedef struct definition {
    char* name;
    value value;
} definition;


typedef struct local {
    int arg_num;
    int local_num;
    int closure_num;
    value* args;
    value* local;
    value* closure;
} local;

typedef struct stackframe {
    char* ret_ip;
    local locals;
} stackframe;

typedef struct state {
    value stack[10000];
    stackframe call_stack[10000];
    value* global;
    int global_num;
    local locals;
    int sp;
    int csp;
} state;

void st_push(state* st, value v) {
    st->stack[st->sp++] = v;
}

value st_top(state* st) {
    return st->stack[st->sp - 1];
}

value st_pop(state* st) {
    return st->stack[--st->sp];
}

void st_update(state* st, int dsgn_type, int dsgn_num, value v) {
    switch (dsgn_type) {
        case dsgn_global:
            st->global[dsgn_num] = v;
            break;
        case dsgn_local:
            st->locals.local[dsgn_num] = v;
            break;
        case dsgn_arg:
            st->locals.args[dsgn_num] = v;
            break;
        case dsgn_access:
            st->locals.closure[dsgn_num] = v;
            break;
    }
}

void update_container(value container, int pos, value v) {
    switch (container.type) {
        case type_sexp: {
            sexp *s = ((sexp *) container.content);
            s->values[pos] = v;
        }
            break;
        case type_string: {
            if (v.type != type_int) return;
            char c = *((long long *) v.content);
            char *str = ((char *) container.content);
            str[pos] = c;
        }
            break;
        case type_array: {
            array *arr = ((array *) container.content);
            arr->values[pos] = v;
        }
            break;
        default:
            break;
    }
}

void st_push_call(state* st, stackframe frame) {
    st->call_stack[st->csp++] = frame;
}

value st_get_value(state* st, designation d) {
    switch (d.type) {
        case dsgn_global:
            return st->global[d.pos];
        case dsgn_local:
            return st->locals.local[d.pos];
        case dsgn_arg:
            return st->locals.args[d.pos];
        case dsgn_access:
            return st->locals.closure[d.pos];
    }
}

long long exec_binop(value xv, value yv, int op) {
    if (op == 9) {
        if (xv.type != type_int || yv.type != type_int) {
            return 0;
        }
    }
    int x = *((long long*)(xv.content));
    int y = *((long long*)(yv.content));
    switch (op) {
        case 0:
            return x + y;
        case 1:
            return x - y;
        case 2: {
            // fprintf(stderr, "multiplying %d and %d", x, y);
            return x * y;
        }
        case 3:
            return x / y;
        case 4:
            return x % y;
        case 5:
            return x < y;
        case 6:
            return x <= y;
        case 7:
            return x > y;
        case 8:
            return x >= y;
        case 9:
            return x == y;
        case 10:
            return x != y;
        case 11:
            return x && y;
        case 12:
            return x || y;
        default:
            return -1;
    }
    return -1;
}

void print_value(FILE* f, value v) {
    // fprintf(f, "\t\t");
    switch (v.type) {
        case type_int:
            // fprintf(f, "INT ");
            // fprintf(f, "%lld", *((long long*)(v.content)));
            break;
        case type_string:
            // fprintf(f, "STR ");
            // fprintf(f, "%d", *((char*)(v.content)));
            break;
        case type_sexp:
            // fprintf(f, "SEXP ");
            break;
        case type_reference:
            // fprintf(f, "REF ");
            break;
        case type_closure:
            // fprintf(f, "CLS ");
            break;
        case type_empty:
            // fprintf(f, "EMPTY ");
            break;
        case type_array:
            // fprintf(f, "ARRAY ");
            break;
    }
    // fprintf (f, "; ");
}

void print_locals(FILE* f, local locals) {
    // fprintf(f, "\tlocal state:\n");
    // fprintf(f, "\targs: %d\n", locals.arg_num);
    for (int i = 0; i < locals.arg_num; i++) {
        print_value(f, locals.args[i]);
    }
    // fprintf(f, "\n\tlocal: %d\n", locals.local_num);
    for (int i = 0; i < locals.local_num; i++) {
        print_value(f, locals.local[i]);
    }
    // fprintf(f, "\n\tclosure: %d\n", locals.closure_num);
    for (int i = 0; i < locals.closure_num; i++) {
        print_value(f, locals.closure[i]);
    }
}

void print_st(FILE *f, state* st) {
    // fprintf (f, "\tstack: %d\n", st->sp);
    for (int i = 0; i < st->sp; i++) {
        print_value(f, st->stack[i]);
    }
    // fprintf(f, "\n\tcall stack size: %d\n", st->csp);
    // fprintf(f, "\tglobal:\n");
    for (int i = 0; i < st->global_num; i++) {
        print_value(f, st->global[i]);
    }
    // fprintf(f, "\n");
    print_locals(f, st->locals);
    // fprintf(f, "\n");
}

void st_begin(state* st, int args_num, int local_num) {
    st->locals.local_num = local_num;
    st->locals.local = malloc(sizeof(value) * local_num);
    for (int i = 0; i < local_num; i++) {
        st->locals.local[i] = empty_value();
    }
}

void elem(state* st) {
    value v = st_pop(st);
    if (v.type == type_int) {
        int index = *((long long*)v.content);
        value container = st_pop(st);
        switch (container.type) {
            case type_sexp: {
                sexp *s = ((sexp *) container.content);
                st_push(st, s->values[index]);
            }
                break;
            case type_string: {
                char *str = ((char *) container.content);
                st_push(st, int_value_of(str[index]));
            }
                break;
            case type_array: {
                array *arr = ((array *) container.content);
                st_push(st, arr->values[index]);
            }
                break;
            default:
                break;
        }
    }
}

void check_array(state* st, int size) {
    value v = st_pop(st);
    if (v.type != type_array) {
        st_push(st, int_value_of(0));
        return;
    }
    int result = (size == ((array*)v.content)->value_num);
    st_push(st, int_value_of(result));
}

char* convert_to_string(value v);

char* concat(char* buf, value* values, int size) {
    int total_len = strlen(buf);

    for (int i = 0; i < size; i++) {
        char* value_str = convert_to_string(values[i]);
        int length = strlen(value_str);
        total_len += length + 2;
        buf = realloc(buf, total_len * sizeof(char));
        strcat(buf, value_str);
        if (i != size - 1) {
            strcat(buf, ", ");
        }
    }
    return buf;
}

char* convert_to_string(value v) {
    if (v.type == type_int) {
        long long* n = v.content;
        int length = snprintf(NULL, 0,"%lld", *n);
        char* buf = malloc((length + 1) * sizeof(char));
        sprintf(buf, "%lld", *n);
        return buf;
    }
    if (v.type == type_string) {
        int l = strlen(v.content);
        char* buf = malloc( (l + 4) * sizeof(char));
        buf[0] = '"';
        strcpy(buf + 1, v.content);
        buf[l + 1] = '"';
        buf[l + 2] = '\0';
        return buf;
    }
    if (v.type == type_array) {
        array* arr = v.content;
        char* buf = malloc(4 * sizeof(char));
        buf[0] = '[';
        buf[1] = '\0';
        int total_len = 4;
        buf = concat(buf, arr->values, arr->value_num);
        strcat(buf, "]");
        return buf;
    }
    if (v.type == type_sexp) {
        sexp* sx = v.content;

        if (!strcmp(sx->name, "cons")) {
            char* buf = malloc(4 * sizeof(char));
            buf[0] = '{';
            buf[1] = '\0';
            int total_len = 4;
            sexp* sxv = sx;
            while (1) {
                if (sxv->arity != 2) break;

                char* value_str = convert_to_string(sxv->values[0]);
                int length = strlen(value_str);
                total_len += length + 2;
                buf = realloc(buf, total_len * sizeof(char));
                strcat(buf, value_str);

                value next = sxv->values[1];
                if (next.type == type_int && *((long long*)next.content) == 0) {
                    break;
                }
                if (next.type != type_sexp) break;
                sexp* sxn = next.content;
                if (strcmp(sxn->name, "cons")) break;
                strcat(buf, ", ");
                sxv = sxn;
            }
            strcat(buf, "}");
            return buf;
        } else {
            char* buf = malloc(strlen(sx->name + 3) * sizeof(char));
            strcpy(buf, sx->name);
            if (sx->arity == 0) return buf;
            strcat(buf, "(");
            buf = concat(buf, sx->values, sx->arity);
            strcat(buf, ")");
            return buf;
        }
    }

    return NULL;
}

void run_builtin(state* st, int opcode, int arg) {
    switch (opcode) {
        case 0: {// READ
            int read_value;
             fprintf(stdout, "> ");
            fscanf(stdin, "%d", &read_value);
            st_push(st, int_value_of(read_value));
            break;
        }
        case 1: { // WRITE
            value top = st_pop(st);
            if (top.type == type_int) {
                  fprintf(stdout, "%lld\n", *((long long*)top.content));
            }
            st_push(st, empty_value());
        }
            break;
        case 2: {// LENGTH
            value container = st_pop(st);
            switch (container.type) {
                case type_sexp: {
                    sexp *s = ((sexp *) container.content);
                    st_push(st, int_value_of(s->arity));
                }
                    break;
                case type_string: {
                    char *str = ((char *) container.content);
                    st_push(st, int_value_of(strlen(str)));
                }
                    break;
                case type_array: {
                    array *arr = ((array *) container.content);
                    st_push(st, int_value_of(arr->value_num));
                }
                    break;
                default:
                    break;
            }
        }
            break;
        case 3: { // STRING
            value v = st_pop(st);
            char* str = convert_to_string(v);
            st_push(st, string_value(str));
        }
            break;
        case 4: { // ARRAY
            array* arr = malloc(sizeof(array));
            arr->value_num = arg;
            arr->values = malloc(sizeof(value) * arg);
            for (int i = arg - 1; i >= 0; i--) {
                arr->values[i] = st_pop(st);
            }
            st_push(st, array_value(arr));
        }
            break;
        case 5: { // ELEM?
            elem(st);
        }
            break;
    }
}

void pattern(state* st, int pat_code) {
    char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
    value x = st_pop(st);
    int res = 0;
    switch (pat_code) {
        case 0: {
            value y = st_pop(st);
            if (x.type != type_string || y.type != type_string) {
                res = 0;
            } else {
                char* xs = (char*)x.content;
                char* ys = (char*)y.content;
                int cmp = strcmp(xs, ys);
                if (cmp == -1) cmp = 1;
                res = !cmp;
            }
            break;
        }
        case 1: {
            res = x.type == type_string;
            break;
        }
        case 2: {
            res = x.type == type_array;
            break;
        }
        case 3: {
            res = x.type == type_sexp;
            break;
        }
        case 4: {
            res = x.type != type_int;
            break;
        }
        case 5: {
            res = x.type == type_int;
            break;
        }
        case 6: {
            res = x.type == type_closure;
            break;
        }
    }
    st_push(st, int_value_of(res));
}


void interpret (FILE *f, bytefile *bf) {

# define INT    (ip += sizeof (int), *(int*)(ip - sizeof (int)))
# define BYTE   *ip++
# define STRING get_string (bf, INT)
# define FAIL   failure ("ERROR: invalid opcode %d-%d\n", h, l)
# define JUMPTO(address) ip = bf->code_ptr + address;

    state st;
    st.sp = 0;
    st.csp = 0;
    st.locals.arg_num = 0;
    st.locals.closure_num = 0;
    st.locals.args = NULL;
    st.locals.closure = NULL;
    st.global_num = bf->global_area_size;
    st.global = malloc(bf->global_area_size * sizeof(value));
    for (int i = 0; i < st.global_num; i++) {
        st.global[i] = empty_value();
    }
    char *ip     = bf->code_ptr;
    char *ops [] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
    char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
    char *lds [] = {"LD\t", "LDA\t", "ST\t"};
    char *builtin [] = {"Lread", "Lwrite", "Lelem", "Llength", "Larray", "Lstring"};
    do {
        char x = BYTE,
                h = (x & 0xF0) >> 4,
                l = x & 0x0F;

        // fprintf (f, "0x%.8x:\t%x%x\t", ip-bf->code_ptr-1, h, l);

        switch (h) {
            case 15:
                goto stop;

                /* BINOP */
            case 0: {
                // fprintf(f, "BINOP\t%s", ops[l - 1]);
                value y = st_pop(&st);
                value x = st_pop(&st);
                int x_not_int = x.type != type_int;
                int y_not_int = y.type != type_int;
                if (l == 10) {
                    if (x_not_int && y_not_int) {
                        FAIL;
                    }
                } else {
                    if (x_not_int || y_not_int) {
                        FAIL;
                    }
                }
                int result = exec_binop(x, y, l - 1);

                st_push(&st, int_value_of(result));
            }
                break;

            case 1:
                switch (l) {
                    case  0: {
                        int v = INT;
                        // fprintf(f, "CONST\t%d", v);
                        st_push(&st, int_value_of(v));
                    }
                        break;

                    case  1: {
                        char *s = STRING;
                        // fprintf(f, "STRING\t%s", s);
                        char *copy = malloc(sizeof(char) * strlen(s));
                        strcpy(copy, s);
                        st_push(&st, string_value(copy));
                    }
                        break;

                    case  2: {
                        char *name = STRING;
                        int n = INT;
                        // fprintf(f, "SEXP\t%s ", name);
                        // fprintf(f, "%d", n);
                        sexp *sx = malloc(sizeof(sexp));
                        sx->name = name;
                        sx->arity = n;
                        for (int i = 0; i < n; i++) {
                            sx->values[i] = st_pop(&st); // TODO order
                        }
                        st_push(&st, sexp_value(sx));
                    }
                        break;

                    case  3: {
                        // fprintf(f, "STI");
                        value v = st.stack[st.sp - 1];
                        value d = st.stack[st.sp - 2];
                        designation* ds = (designation*) d.content;
                        st_update(&st, ds->type, ds->pos, v);
                        st.stack[st.sp - 2] = st.stack[st.sp - 1];
                        st.sp--;
                    }
                        break;
                    case  4: {
                        value v = st_pop(&st);
                        // fprintf(f, "STA");
                        value j = st_pop(&st);
                        if (j.type == type_reference) {
                            designation *d = (designation *) v.content;
                            st_update(&st, d->type, d->pos, v);
                        }
                        if (j.type == type_int) {
                            int pos = *((long long*) j.content);
                            value container = st_pop(&st);
                            update_container(container, pos, v);
                        }
                        st_push(&st, v);
                    }
                        break;

                    case  5: {
                        int new_ip = INT;
                        // fprintf(f, "JMP\t0x%.8x", new_ip);
                        JUMPTO(new_ip);
                    }
                        break;

                    case  6: {
                        // fprintf(f, "END");
                        if (st.csp == 0) {
                            goto stop;
                        }
                        stackframe frame = st.call_stack[--st.csp]; // same as RET
                        st.locals = frame.locals;
                        ip = frame.ret_ip;
                    }
                        break;

                    case  7: {
                        // fprintf(f, "RET");
                        if (st.csp == 0) {
                            goto stop;
                        }
                        stackframe frame = st.call_stack[--st.csp];
                        st.locals = frame.locals;
                        ip = frame.ret_ip;
                    }
                        break;

                    case  8: {
                        // fprintf(f, "DROP");
                        st.sp--;
                    }
                        break;

                    case  9: {
                        // fprintf(f, "DUP");
                        st.stack[st.sp] = st.stack[st.sp - 1];
                        st.sp++;
                    }
                        break;

                    case 10: {
                        // fprintf(f, "SWAP");
                        value t = st.stack[st.sp - 1];
                        st.stack[st.sp - 1] = st.stack[st.sp - 2];
                        st.stack[st.sp - 2] = t;
                    }
                        break;
                    case 11: {
                        // fprintf(f, "ELEM");
                        elem(&st);
                    }
                    default:
                        FAIL;
                }
                break;

            case 2:
            case 3:
            case 4: {
                // fprintf(f, "%s\t", lds[h - 2]);
                int dsgn_num = INT;
                designation d;
                d.type = l;
                d.pos = dsgn_num;
                switch (l) {
                    case 0:
                        // fprintf(f, "G(%d)", dsgn_num);
                        break;
                    case 1:
                        // fprintf(f, "L(%d)", dsgn_num);
                        break;
                    case 2:
                        // fprintf(f, "A(%d)", dsgn_num);
                        break;
                    case 3:
                        // fprintf(f, "C(%d)", dsgn_num);
                        break;
                    default:
                        FAIL;
                }

                switch (h - 2) {
                    case 0: // LD
                        st_push(&st, st_get_value(&st, d));
                        break;
                    case 1: {// LDA
                        designation* d = malloc(sizeof(designation));
                        d->type = l;
                        d->pos = dsgn_num;
                        st.stack[st.sp++] = ref_value(d);
                    }
                        break;
                    case 2: //ST
                        st_update(&st, l, dsgn_num, st_top(&st));
                        break;
                }
            }
                break;

            case 5:
                switch (l) {
                    case  0: {
                        int new_ip = INT;
                        // fprintf(f, "CJMPz\t%0x.8x", new_ip);
                        if (to_int(st.stack[--st.sp]) == 0) { // TODO checks
                            JUMPTO(new_ip);
                        }
                    }
                        break;

                    case  1: {
                        int new_ip = INT;
                        // fprintf(f, "CJMPnz\t%0x.8x", new_ip);
                        if (to_int(st.stack[--st.sp]) != 0) { // TODO checks
                            JUMPTO(new_ip);
                        }
                    }
                        break;

                    case  2: {
                        int args_num = INT;
                        int local_num = INT;
                        // fprintf(f, "BEGIN\t%d ", args_num);
                        // fprintf(f, "%d", local_num);
                        st_begin(&st, args_num, local_num);
                    }
                        break;

                    case  3: {
                        int args_num = INT;
                        int local_num = INT;
                        // fprintf(f, "CBEGIN\t%d ", args_num);
                        // fprintf(f, "%d", local_num);
                        st_begin(&st, args_num, local_num);
                    }
                        break;

                    case  4: {
                        int code_ip = INT;
                        // fprintf(f, "CLOSURE\t0x%.8x", code_ip);
                        int n = INT;
                        closure *cls = malloc(sizeof(closure));
                        cls->value_num = n;
                        cls->values = malloc(sizeof(value) * n);
                        cls->ip = code_ip;
                        for (int i = 0; i < n; i++) {
                            designation d;
                            d.type = BYTE;
                            if (d.type < 0 || d.type > 3) {
                                FAIL;
                            }
                            d.pos = INT;
                            cls->values[i] = st_get_value(&st, d);
                            switch (d.type) {
                                case 0:
                                    // fprintf(f, "G(%d)", d.pos);
                                    break;
                                case 1:
                                    // fprintf(f, "L(%d)", d.pos);
                                    break;
                                case 2:
                                    // fprintf(f, "A(%d)", d.pos);
                                    break;
                                case 3:
                                    // fprintf(f, "C(%d)", d.pos);
                                    break;
                                default:
                                    FAIL;
                            }
                        }
                        value closure = closure_value(cls);
                        st_push(&st, closure);
                    }
                        break;

                    case  5: {
                        int arg_num = INT;
                        // fprintf(f, "CALLC\t%d", arg_num);
                        stackframe frame;
                        frame.ret_ip = ip;
                        frame.locals = st.locals;

                        value v = st_pop(&st);
                        if (v.type != type_closure) {
                            FAIL;
                        }

                        closure* cls = (closure*) v.content;
                        int closure_num = cls->value_num;

                        st.locals.arg_num = arg_num;
                        st.locals.local_num = 0;
                        st.locals.closure_num = closure_num;

                        st.locals.args = malloc(sizeof(value) * arg_num);
                        st.locals.local = NULL;
                        st.locals.closure = malloc(sizeof(value) * closure_num);

                        for (int i = 0; i < arg_num; i++) {
                            st.locals.args[i] = st_pop(&st); // TODO order;
                        }

                        for (int i = 0; i < cls->value_num; i++) {
                            st.locals.closure[i] = cls->values[i];
                        }
                        // locals locals empty
                        st_push_call(&st, frame);
                        JUMPTO(cls->ip);
                    }
                        break;

                    case  6: {
                        int fn_ip = INT;
                        int arg_num = INT;
                        // fprintf(f, "CALL\t%0x%.8x ", fn_ip);
                        // fprintf(f, "%d", arg_num);
                        // todo how to find by name and where is name?
                        // if found:
                        stackframe frame;
                        frame.ret_ip = ip;
                        frame.locals = st.locals;

                        st.locals.arg_num = arg_num;
                        st.locals.local_num = 0;
                        st.locals.closure_num = 0;

                        st.locals.args = malloc(sizeof(value) * arg_num);
                        st.locals.local = NULL;
                        st.locals.closure = NULL;

                        for (int i = 0; i < arg_num; i++) {
                            st.locals.args[i] = st_pop(&st); // TODO order;
                        }
                        st_push_call(&st, frame);
                        JUMPTO(fn_ip);
                    }
                        break;

                    case  7: {
                        char *name = STRING;
                        int arity = INT;
                        // fprintf(f, "TAG\t%s ", name);
                        // fprintf(f, "%d", arity);
                    }
                        break;

                    case  8: {
                        int size = INT;
                        // fprintf(f, "ARRAY\t%d", size);
                        check_array(&st, size);
                    }
                        break;

                    case  9: {
                        int x1 = INT;
                        int x2 = INT;
                       // fprintf (f, "FAIL\t%d", x1);
                       // fprintf (f, "%d", x2);
                        FAIL;
                    }
                        break;

                    case 10: {
                        int line_num = INT;
                       // fprintf (f, "LINE\t%d", line_num);
                    }
                        break;

                    default:
                        FAIL;
                }
                break;

            case 6:
                // fprintf (f, "PATT\t%s", pats[l]);
                pattern(&st, l);
                break;
            case 7: {
                int arg = -1;
                if (l == 4) {
                    arg = INT;
                }
                // fprintf(f, "CALL %s %d\t", builtin[l], arg);
                run_builtin(&st, l, arg);
            }
                break;
            default:
                FAIL;
        }

        // fprintf (f, "\n");
        print_st(f, &st);
    }
    while (1);
    stop: {
       // fprintf(f, "<end>\n");
        0;
    }
}

int main (int argc, char* argv[]) {
    bytefile *f = read_file (argv[1]);
    interpret (stderr, f);
    return 0;
}
