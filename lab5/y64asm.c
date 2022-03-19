#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y64asm.h"

line_t *line_head = NULL;
line_t *line_tail = NULL;
int lineno = 0;

#define err_print(_s, _a...)            \
    do                                  \
    {                                   \
        if (lineno < 0)                 \
            fprintf(stderr, "[--]: "_s  \
                            "\n",       \
                    ##_a);              \
        else                            \
            fprintf(stderr, "[L%d]: "_s \
                            "\n",       \
                    lineno, ##_a);      \
    } while (0);

int64_t vmaddr = 0; /* vm addr */

/* register table */
const reg_t reg_table[REG_NONE] = {
    {"%rax", REG_RAX, 4},
    {"%rcx", REG_RCX, 4},
    {"%rdx", REG_RDX, 4},
    {"%rbx", REG_RBX, 4},
    {"%rsp", REG_RSP, 4},
    {"%rbp", REG_RBP, 4},
    {"%rsi", REG_RSI, 4},
    {"%rdi", REG_RDI, 4},
    {"%r8", REG_R8, 3},
    {"%r9", REG_R9, 3},
    {"%r10", REG_R10, 4},
    {"%r11", REG_R11, 4},
    {"%r12", REG_R12, 4},
    {"%r13", REG_R13, 4},
    {"%r14", REG_R14, 4}};
const reg_t *find_register(char *name)
{
    int i;
    for (i = 0; i < REG_NONE; i++)
        if (!strncmp(name, reg_table[i].name, reg_table[i].namelen))
            return &reg_table[i];
    return NULL;
}

/* instruction set */
const instr_t instr_set[] = {
    {"nop", 3, HPACK(I_NOP, F_NONE), 1},
    {"halt", 4, HPACK(I_HALT, F_NONE), 1},
    {"rrmovq", 6, HPACK(I_RRMOVQ, F_NONE), 2},
    {"cmovle", 6, HPACK(I_RRMOVQ, C_LE), 2},
    {"cmovl", 5, HPACK(I_RRMOVQ, C_L), 2},
    {"cmove", 5, HPACK(I_RRMOVQ, C_E), 2},
    {"cmovne", 6, HPACK(I_RRMOVQ, C_NE), 2},
    {"cmovge", 6, HPACK(I_RRMOVQ, C_GE), 2},
    {"cmovg", 5, HPACK(I_RRMOVQ, C_G), 2},
    {"irmovq", 6, HPACK(I_IRMOVQ, F_NONE), 10},
    {"rmmovq", 6, HPACK(I_RMMOVQ, F_NONE), 10},
    {"mrmovq", 6, HPACK(I_MRMOVQ, F_NONE), 10},
    {"addq", 4, HPACK(I_ALU, A_ADD), 2},
    {"subq", 4, HPACK(I_ALU, A_SUB), 2},
    {"andq", 4, HPACK(I_ALU, A_AND), 2},
    {"xorq", 4, HPACK(I_ALU, A_XOR), 2},
    {"jmp", 3, HPACK(I_JMP, C_YES), 9},
    {"jle", 3, HPACK(I_JMP, C_LE), 9},
    {"jl", 2, HPACK(I_JMP, C_L), 9},
    {"je", 2, HPACK(I_JMP, C_E), 9},
    {"jne", 3, HPACK(I_JMP, C_NE), 9},
    {"jge", 3, HPACK(I_JMP, C_GE), 9},
    {"jg", 2, HPACK(I_JMP, C_G), 9},
    {"call", 4, HPACK(I_CALL, F_NONE), 9},
    {"ret", 3, HPACK(I_RET, F_NONE), 1},
    {"pushq", 5, HPACK(I_PUSHQ, F_NONE), 2},
    {"popq", 4, HPACK(I_POPQ, F_NONE), 2},
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1},
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2},
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4},
    {".quad", 5, HPACK(I_DIRECTIVE, D_DATA), 8},
    {".pos", 4, HPACK(I_DIRECTIVE, D_POS), 0},
    {".align", 6, HPACK(I_DIRECTIVE, D_ALIGN), 0},
    {NULL, 1, 0, 0} // end
};

instr_t *find_instr(char *name)
{
    int i;
    for (i = 0; instr_set[i].name; i++)
        if (strncmp(instr_set[i].name, name, instr_set[i].len) == 0)
            return &instr_set[i];
    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{   
    symbol_t *p = symtab->next;
    while (p != NULL)   //search
    {
        // printf("result: %d\n", strcmp(name, p->name));
        // printf("name: %s\n", name);
        // printf("p->name: %s\n", p->name);
        //comare two string
        if (strcmp(name, p->name) == 0) //same
            return p;
        //update p
        p = p->next;
    }
    return NULL;
}

/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)
{
    /* check duplicate */
    if (find_symbol(name) != NULL)
        return -1;

    /* create new symbol_t (don't forget to free it)*/
    symbol_t *sym = (symbol_t *) malloc(sizeof(symbol_t));
    sym->name = name;
    sym->next = NULL;

    /* add the new symbol_t to symbol table */
    symbol_t *p = symtab;
    
    while (p != NULL) {  //add      
        if (p->next == NULL) {
            p->next = sym;
            break;
        }
        p = p->next;
    }

    return 0;
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 */
void add_reloc(char *name, bin_t *bin)
{
    /* create new reloc_t (don't forget to free it)*/
    reloc_t *rel = (reloc_t *) malloc(sizeof(reloc_t));
    rel->name = name;
    rel->y64bin = bin;
    rel->next = NULL;

    /* add the new reloc_t to relocation table */
    reloc_t *p = reltab;
    if (!reltab)
        reltab = rel;
    while (!p)
    {
        if (!p->next)
            p->next = rel;
        p = p->next;
    }
}

/* macro for parsing y64 assembly code */
#define IS_DIGIT(s) ((*(s) >= '0' && *(s) <= '9') || *(s) == '-' || *(s) == '+')
#define IS_LETTER(s) ((*(s) >= 'a' && *(s) <= 'z') || (*(s) >= 'A' && *(s) <= 'Z'))
#define IS_COMMENT(s) (*(s) == '#')
#define IS_REG(s) (*(s) == '%')
#define IS_IMM(s) (*(s) == '$')

#define IS_BLANK(s) (*(s) == ' ' || *(s) == '\t')
#define IS_END(s) (*(s) == '\0')

#define SKIP_BLANK(s)                     \
    do                                    \
    {                                     \
        while (!IS_END(s) && IS_BLANK(s)) \
            (s)++;                        \
    } while (0);

/* return value from different parse_xxx function */
typedef enum
{
    PARSE_ERR = -1,
    PARSE_REG,
    PARSE_DIGIT,
    PARSE_SYMBOL,
    PARSE_MEM,
    PARSE_DELIM,
    PARSE_INSTR,
    PARSE_LABEL
} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovq')
 * args
 *     ptr: point to the start of string
 *     inst: point to the inst_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
    /* skip the blank */
    SKIP_BLANK(*ptr);

    /* find_instr and check end */
    char instName[10];
    int i = 0;
    for (; i < 10; ++i) {
        //store instr
        if (IS_LETTER((*ptr) + i) || *((*ptr) + i) == '.') 
            instName[i] = (*ptr)[i];
        //when encounter other letter, end and update *ptr
        else {
            instName[i] = '\0';
            break;
        }
    }

    if (!find_instr(instName))
        return PARSE_ERR;

    /* set 'ptr' and 'inst' */
    (*ptr) += i;    //set ptr
    *inst = find_instr(instName);   //set inst
    return PARSE_INSTR;


}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);   //skip blank
    if (**ptr != delim)  //check
        return PARSE_ERR;
    
    /* set 'ptr' */
    //printf("%c\n", **ptr);
    //if (!IS_END((*ptr) + 1))
    (*ptr)++;
    return PARSE_DELIM;
}

/*
 * parse_reg: parse an expected register token (e.g., '%rax')
 * args
 *     ptr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token,
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    if (!IS_REG(*ptr))
        return PARSE_ERR;

    /* find register */
    char reg_name[5];
    int i = 0;
    for (; i < 5; ++i) {
        if ((*ptr)[i] != ' ' && (*ptr)[i] != ','
            && (*ptr)[i] != ')')
            reg_name[i] = (*ptr)[i];
        else {
            reg_name[i] = '\0';
            break;
        }
    }

    /* set 'ptr' and 'regid' */
    if (!find_register(reg_name))
        return PARSE_ERR;
    else {
        (*ptr) += i;
        *regid = find_register(reg_name)->id;
        return PARSE_REG;
    }
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    char *symbolName = (char *) malloc(20);
    int i = 0;
    //get symbol(char *)
    for (; i < 20; ++i) {
        if (IS_LETTER((*ptr)+i) || IS_DIGIT((*ptr)+i))
            symbolName[i] = (*ptr)[i];
        else {
            symbolName[i] = '\0';
            break;
        }
    }

    /* allocate name and copy to it */
    *name = symbolName;

    /* set 'ptr' and 'name' */
    (*ptr) += i;
    return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *value)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);   //skip blanck
    if (!IS_DIGIT(*ptr))    //check
        return PARSE_ERR;
    char value_char[20];    //get value
    int i = 0; int base = 10;
    for (; i < 20; ++i) {
        if ((*ptr)[i] == 'x') //check if hexdecimal
            base = 16;     
        if ((*ptr)[i] != ',' && (*ptr)[i] != ' ' 
            && (*ptr)[i] != '(' && (*ptr)[i] != '\0') {
            value_char[i] = (*ptr)[i];
        }
        else {
            value_char[i] = '\0';
            break;
        }
    }
    
    /* calculate the digit, (NOTE: see strtoll()) */
    /* set 'ptr' and 'value' */
    char *end = NULL;      //calculate
    (*ptr) += i;        //set ptr
    *value = strtoul(value_char, &end, base);    //set value
    //printf("hex: 0x%lx\n", *value);
    return PARSE_DIGIT;
}

/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);   //skip
    if (!IS_IMM(*ptr) && !IS_LETTER(*ptr))  //check
        return PARSE_ERR;

    /* if IS_IMM, then parse the digit */
    if (IS_IMM(*ptr)) {
        parse_delim(ptr, '$');
        return parse_digit(ptr, value);
    }
    
    /* if IS_LETTER, then parse the symbol */
    if (IS_LETTER(*ptr)) {
        return parse_symbol(ptr, name);
    }
}

/*
 * parse_mem: parse an expected memory token (e.g., '8(%rbp)')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_MEM: success, move 'ptr' to the first char after token,
 *                          and store the value of digit to 'value',
 *                          and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr', 'value' and 'regid' are undefined
 */
parse_t parse_mem(char **ptr, long *value, regid_t *regid)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    /* calculate the digit and register, (ex: (%rbp) or 8(%rbp)) */
    /* set 'ptr', 'value' and 'regid' */
    if (**ptr == '(') {     //(%rbp)
        *value = 0;     //set value = 0
        if (parse_delim(ptr, '(') == PARSE_ERR) //skip '('
            return PARSE_ERR;
        if (parse_reg(ptr, regid) == PARSE_ERR) //parse %rbp
            return PARSE_ERR;
        if (parse_delim(ptr, ')') == PARSE_ERR) //skip ')'
            return PARSE_ERR;
    }
    else if (IS_DIGIT(*ptr)) {      //8(%rbp)
        if (parse_digit(ptr, value) == PARSE_ERR)   //parse 8
            return PARSE_ERR;
        if (parse_delim(ptr, '(') == PARSE_ERR) //skip '('
            return PARSE_ERR;
        if (parse_reg(ptr, regid) == PARSE_ERR) //parse %rbp
            return PARSE_ERR;
        if (parse_delim(ptr, ')') == PARSE_ERR) //skip ')'
            return PARSE_ERR; 
    }
    else return PARSE_ERR;      //error
    
    return PARSE_MEM;
}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    if (!IS_DIGIT(*ptr) && !IS_LETTER(*ptr))
        return PARSE_ERR;
    /* if IS_DIGIT, then parse the digit */
    else if (IS_DIGIT(*ptr)) {
        return parse_digit(ptr, value);
    }
    /* if IS_LETTER, then parse the symbol */
    else if (IS_LETTER(*ptr)) {
        return parse_symbol(ptr, name);
    }
    /* set 'ptr', 'name' and 'value' */

}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);   //skip blank
    if (!IS_LETTER(*ptr))   //check
        return PARSE_ERR;
    /* allocate name and copy to it */
    if (parse_symbol(ptr, name) == PARSE_ERR)   //parse 'Loop'
        return PARSE_ERR;
    if (parse_delim(ptr, ':') == PARSE_ERR)  //skip ':'
        return PARSE_ERR;
    return PARSE_LABEL;

    
    /* set 'ptr' and 'name' */
}

/*
 * parse_line: parse a line of y64 code (e.g., 'Loop: mrmovq (%rcx), %rsi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y64 assembly code
 *
 * return
 *     TYPE_XXX: success, fill line_t with assembled y64 code
 *     TYPE_ERR: error, try to print err information
 */
type_t parse_line(line_t *line)
{

    /* when finish parse an instruction or lable, we still need to continue check
     * e.g.,
     *  Loop: mrmovl (%rbp), %rcx
     *           call SUM  #invoke SUM function */

    /* skip blank and check IS_END */
    char *ptr = line->y64asm;
    SKIP_BLANK(ptr);    //skip
    if (IS_END(ptr))    //check
        return TYPE_ERR;
    /* is a comment ? */
    if (IS_COMMENT(ptr)) {
        line->type = TYPE_COMM;
        return TYPE_COMM;
    }
    /* is a label ? */
    int i = 0;
    bool_t is_label = FALSE;
    while (!IS_END(ptr + i)) {  //check if label(have ':')
        if (ptr[i] == ':') {
            is_label = TRUE;
            break;
        }
        ++i;   
    }
    
    char *labelName = NULL;
    if (is_label && parse_label(&ptr, &labelName) != PARSE_ERR) {   //if label
        //printf("label name: %s\n", labelName);
        add_symbol(labelName);
        find_symbol(labelName)->addr = vmaddr;
        printf("addr: %d\n", find_symbol(labelName)->addr);
        return TYPE_INS;
    }
        
    /* is an instruction ? */
    instr_t *instr = NULL;
    if (parse_instr(&ptr, &instr) != PARSE_ERR) {
        line->type = TYPE_INS;  //set type
        /* set bin */
        line->y64bin.addr = vmaddr; //set addr
        vmaddr += instr->bytes;     //update vmaddr
        line->y64bin.bytes = instr->bytes;  //set bytes
        itype_t icode = HIGH(instr->code);   //get icode
        dtv_t ifun = LOW(instr->code);      //get ifun
        if (icode != I_DIRECTIVE)
            line->y64bin.codes[0] = instr->code;    //set icode + ifun

        switch (icode) {    //parse different instr
            //icode: I_HALT I_NOP
            case I_HALT: 
            case I_NOP:
                break;
            //icode: I_RRMOVQ I_ALU
            case I_RRMOVQ: 
            case I_ALU: {
                regid_t regidA, regidB;
                if (parse_reg(&ptr, &regidA) == PARSE_ERR)  //regA
                    return TYPE_ERR;
                if (parse_delim(&ptr, ',') == PARSE_ERR) //skip ','
                    return TYPE_ERR;
                if (parse_reg(&ptr, &regidB) == PARSE_ERR) //regB
                    return TYPE_ERR;
                line->y64bin.codes[1] = HPACK(regidA, regidB);  //set reg in codes[]
                break;
            }
            //icode: I_IRMOVQ
            case I_IRMOVQ: {
                regid_t regid;
                int64_t imm = 0;
                char *name = NULL;
                if (parse_imm(&ptr, &name, &imm)== PARSE_ERR)  //imm
                    return TYPE_ERR;
                //printf("imm: %d\n", imm);
                if (parse_delim(&ptr, ',') == PARSE_ERR) //skip ','
                    return TYPE_ERR;
                if (parse_reg(&ptr, &regid) == PARSE_ERR) //reg
                    return TYPE_ERR;
                
                line->y64bin.codes[1] = HPACK(REG_NONE, regid);   //pack 0xF and reg
                if (name != NULL) { //if symbol, set imm to its address
                    symbol_t *sym = find_symbol(name);
                    imm = sym->addr;
                }
                for (int i = 2; i < 10; ++i) {   //set imm in codes[]
                        line->y64bin.codes[i] = imm;
                        imm >>= 8;
                    }
                break;
            }
            //icode: I_RMMOVQ
            case I_RMMOVQ: {
                regid_t regidA, regidB;
                int64_t valC;
                if (parse_reg(&ptr, &regidA) == PARSE_ERR)  //regA
                    return TYPE_ERR;
                if (parse_delim(&ptr, ',') == PARSE_ERR) //skip ','
                    return TYPE_ERR;
                if (parse_mem(&ptr, &valC, &regidB) == PARSE_ERR) //D(rB)
                    return TYPE_ERR;
                line->y64bin.codes[1] = HPACK(regidA, regidB);  //set reg in codes[]
                for (int i = 2; i < 10; ++i) {   //set valC in codes[]
                    line->y64bin.codes[i] = valC;
                    valC  >>= 8;
                }
                break;
            }
            //icode: I_MRMOVQ
            case I_MRMOVQ: {
                regid_t regidA, regidB;
                int64_t valC;
                if (parse_mem(&ptr, &valC, &regidB) == PARSE_ERR) //D(rB)
                    return TYPE_ERR;
                if (parse_delim(&ptr, ',') == PARSE_ERR) //skip ','
                    return TYPE_ERR;
                if (parse_reg(&ptr, &regidA) == PARSE_ERR)  //regA
                    return TYPE_ERR;
                line->y64bin.codes[1] = HPACK(regidA, regidB);  //set reg in codes[]
                for (int i = 2; i < 10; ++i) {   //set valC in codes[]
                    line->y64bin.codes[i] = valC;
                    valC  >>= 8;
                }
                break;
            }
            //icode: I_JMP I_CALL
            case I_JMP:
            case I_CALL: {
                int64_t valC;
                char *dst = NULL;
                if (parse_data(&ptr, &dst, &valC) == PARSE_ERR)  //valC
                    return TYPE_ERR;
                if (dst != NULL) {      //symbol
                    valC = find_symbol(dst)->addr;
                }
                for (int i = 2; i < 10; ++i) {   //set valC in codes[]
                    line->y64bin.codes[i] = valC & 0xff;
                    valC  >>= 8;
                }
                break;
            }
            //icode: I_RET
            case I_RET:
                break;
            //icode: I_PUSHQ I_POPQ
            case I_PUSHQ:
            case I_POPQ: {
                regid_t regidA;
                if (parse_reg(&ptr, &regidA) == PARSE_ERR)  //regA
                    return TYPE_ERR;
                line->y64bin.codes[1] = HPACK(regidA, REG_NONE);  //set reg in codes[]
                break;
            }
            //icode: I_DIRECTIVE
            case I_DIRECTIVE: {
                switch (ifun) {     //parse directive code
                    //ifun: D_DATA
                    case D_DATA: {
                        int64_t val;
                        if (parse_digit(&ptr, &val) == PARSE_ERR)   //get val
                            return TYPE_ERR;
                        for (int i = 0; i < (line->y64bin).bytes; ++i) {    //set val in code[]
                            (line->y64bin).codes[i] = val & 0xff;
                            val >>= 8; 
                        }
                        break;
                    }
                    //ifun: D_POS
                    case D_POS: {
                        int64_t val;
                        if (parse_digit(&ptr, &val) == PARSE_ERR)   //get val
                            return TYPE_ERR;
                        (line->y64bin).addr = val;  //set addr
                        vmaddr = val;
                        break;
                    }
                    //ifun: D_ALIGN
                    case D_ALIGN: {
                        int64_t gap;
                        if (parse_digit(&ptr, &gap) == PARSE_ERR)   //get val
                            return TYPE_ERR;
                        vmaddr = (vmaddr + gap - 1) / gap * gap;    //set vmaddr
                        break;
                    }           
                }
            }         
        }           
    }
    else return TYPE_ERR;

    return TYPE_INS;
    /* set type and y64bin */
    /* update vmaddr */
    /* parse the rest of instruction according to the itype */
    
}

/*
 * assemble: assemble an y64 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y64 assembly file)
 *
 * return
 *     0: success, assmble the y64 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y64asm;

    /* read y64 code line-by-line, and parse them to generate raw y64 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL)
    {
        slen = strlen(asm_buf);
        while ((asm_buf[slen - 1] == '\n') || (asm_buf[slen - 1] == '\r'))
        {
            asm_buf[--slen] = '\0'; /* replace terminator */
        }

        /* store y64 assembly code */
        y64asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y64asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        line->type = TYPE_COMM;
        line->y64asm = y64asm;
        line->next = NULL;

        line_tail->next = line;
        line_tail = line;
        lineno++;

        if (parse_line(line) == TYPE_ERR)
        {
            return -1;
        }
    }

    lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y64 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
    reloc_t *rtmp = NULL;

    rtmp = reltab->next;
    while (rtmp)
    {
        /* find symbol */
        symbol_t *sym = find_symbol(rtmp->name);
        if (!sym)   //check
            return -1;
        /* relocate y64bin according itype */
        
        /* next */
        rtmp = rtmp->next;
    }
    return 0;
}

/*
 * binfile: generate the y64 binary file
 * args
 *     out: point to output file (an y64 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
    /* prepare image with y64 binary code */
    line_t *line = line_head->next;

    while (line != NULL) {
        if (line->type == TYPE_INS) {
            if (fseek(out, line->y64bin.addr, SEEK_SET) != 0)
            //set bias in the file since the binary in not continuous
                return -1;
            fwrite(line->y64bin.codes, 1, line->y64bin.bytes, out);
        }
        line = line->next;
    }

    /* binary write y64 code to output file (NOTE: see fwrite()) */
    return 0;
}

/* whether print the readable output to screen or not ? */
bool_t screen = FALSE;

static void hexstuff(char *dest, int value, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        char c;
        int h = (value >> 4 * i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len - i - 1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[64];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS)
    {
        bin_t *y64bin = &line->y64bin;
        int i;

        strcpy(buf, "  0x000:                      | ");
        printf("y64bin->addr: %d\n", y64bin->addr);
        hexstuff(buf + 4, y64bin->addr, 3);
        if (y64bin->bytes > 0)
            for (i = 0; i < y64bin->bytes; i++)
                hexstuff(buf + 9 + 2 * i, y64bin->codes[i] & 0xFF, 2);
    }
    else
    {
        strcpy(buf, "                              | ");
    }

    printf("%s%s\n", buf, line->y64asm);
}

/*
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = line_head->next;
    while (tmp != NULL)
    {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    line_head = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(line_head, 0, sizeof(line_t));
    line_tail = line_head;
    lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do
    {
        rtmp = reltab->next;
        if (reltab->name)
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);

    symbol_t *stmp = NULL;
    do
    {
        stmp = symtab->next;
        if (symtab->name)
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do
    {
        ltmp = line_head->next;
        if (line_head->y64asm)
            free(line_head->y64asm);
        free(line_head);
        line_head = ltmp;
    } while (line_head);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;

    if (argc < 2)
        usage(argv[0]);

    if (argv[nextarg][0] == '-')
    {
        char flag = argv[nextarg][1];
        switch (flag)
        {
        case 'v':
            screen = TRUE;
            nextarg++;
            break;
        default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg]) - 3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg] + rootlen, ".ys"))
        usage(argv[0]);

    if (rootlen > 500)
    {
        err_print("File name too long");
        exit(1);
    }

    /* init */
    init();

    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname + rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in)
    {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }

    if (assemble(in) < 0)
    {
        err_print("Assemble y64 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);

    /* relocate binary code */
    if (relocate() < 0)
    {
        err_print("Relocate binary code error");
        exit(1);
    }

    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname + rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out)
    {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0)
    {
        err_print("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);

    /* print to screen (.yo file) */
    if (screen)
        print_screen();

    /* finit */
    finit();
    return 0;
}
