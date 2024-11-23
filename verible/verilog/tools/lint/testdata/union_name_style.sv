// Union names should be lower_snake_case and end with '_t'
typedef union {
    int val;
    logic [8:0] pointer;
} camelUnion;
