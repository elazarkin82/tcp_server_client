/*
 * useful_debug.h
 *
 *  Created on: 6 Mar 2023
 *      Author: elazarkin
 */

#ifndef TESTS_HELPFUL_H_
#define TESTS_HELPFUL_H_

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <execinfo.h>
#include <stdlib.h>

char* addr2line_format(void* addr, char* symbol, char* buffer, int nn_buffer, const char *program)
{
    char cmd[512] = {0};
    int r0 = snprintf(cmd, sizeof(cmd), "addr2line -C -p -s -f -a -e %s %p", program, addr);
    if (r0 < 0 || r0 >= sizeof(cmd)) return symbol;

    FILE* fp = popen(cmd, "r");
    if (!fp) return symbol;

    char* p = fgets(buffer, nn_buffer, fp);
    pclose(fp);

    if (p == NULL) return symbol;
    if ((r0 = strlen(p)) == 0) return symbol;

    // Trait the last newline if exists.
    if (p[r0 - 1] == '\n') p[r0 - 1] = '\0';

    // Find symbol not match by addr2line, like
    //      0x0000000000021c87: ?? ??:0
    //      0x0000000000002ffa: _start at ??:?
    for (p = buffer; p < buffer + r0 - 1; p++) {
        if (p[0] == '?' && p[1] == '?') return symbol;
    }

    return buffer;
}

void* parse_symbol_offset(char* frame)
{
    char* p = NULL;
    char* p_symbol = NULL;
    int nn_symbol = 0;
    char* p_offset = NULL;
    int nn_offset = 0;

    // Read symbol and offset, for example:
    //      /tools/backtrace(foo+0x1820) [0x555555555820]
    for (p = frame; *p; p++) {
        if (*p == '(') {
            p_symbol = p + 1;
        } else if (*p == '+') {
            if (p_symbol) nn_symbol = p - p_symbol;
            p_offset = p + 1;
        } else if (*p == ')') {
            if (p_offset) nn_offset = p - p_offset;
        }
    }
    if (!nn_symbol && !nn_offset) {
        return NULL;
    }

    // Convert offset(0x1820) to pointer, such as 0x1820.
    char tmp[128];
    if (!nn_offset || nn_offset >= sizeof(tmp)) {
        return NULL;
    }

    int r0 = EOF;
    void* offset = NULL;
    tmp[nn_offset] = 0;
    if ((r0 = sscanf(strncpy(tmp, p_offset, nn_offset), "%p", &offset)) == EOF) {
        return NULL;
    }

    // Covert symbol(foo) to offset, such as 0x2fba.
    if (!nn_symbol || nn_symbol >= sizeof(tmp)) {
        return offset;
    }

    void* object_file;
    if ((object_file = dlopen(NULL, RTLD_LAZY)) == NULL) {
        return offset;
    }

    void* address;
    tmp[nn_symbol] = 0;
    if ((address = dlsym(object_file, strncpy(tmp, p_symbol, nn_symbol))) == NULL) {
        dlclose(object_file);
        return offset;
    }

    Dl_info symbol_info;
    if ((r0 = dladdr(address, &symbol_info)) == 0) {
        dlclose(object_file);
        return offset;
    }

    dlclose(object_file);
    return (void*)((uintptr_t)symbol_info.dli_saddr - (uintptr_t)symbol_info.dli_fbase + (uintptr_t)offset);
}

void print_stack(const char* program, const char* log_file_path)
{
	FILE *log_file = fopen(log_file_path, "w");
    void* addresses[64];
    int nn_addresses = backtrace(addresses, sizeof(addresses) / sizeof(void*));
    char** symbols = backtrace_symbols(addresses, nn_addresses);
    char buffer[128];
    printf("\nframes:\n");
    for (int i = 0; i < nn_addresses; i++)
    {
        void* frame = parse_symbol_offset(symbols[i]);
        char* fmt = addr2line_format(frame, symbols[i], buffer, sizeof(buffer), program);
        int parsed = (fmt == buffer);
        printf("%p %d %s\n", frame, parsed, fmt);
        fprintf(log_file, "%p ", frame);
    }

    fclose(log_file);
    free(symbols);
}

#endif /* TESTS_HELPFUL_H_ */
