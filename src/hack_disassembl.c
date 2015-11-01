/*
 * * author：liyang
 * * encode: utf-8
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "nl_hack.h"
#include "hack_disassembl.h"

static unsigned g_objdump_num = 0;
static unsigned g_grep_num = 0;

/* 下面的正则表达式用于过滤in指令的调用 */
#define REGEXP_STRING "^\\s*[0-9, a-f]*:\\s*e5\\s\\(\\([0-9, a-f]\\{2\\}\\)\\s\\)*\\s*in"

/*
 * 功  能： 通过objdump对可执行文件反汇编，并使用grep过滤器输出结果
 * 参  数： 同executable_file_check
 * 返回值： 同executable_file_check
 */
int disassembl_check_in(const char *filename, char **out)
{
    int ret = 0;
    char cmdbuf[1024] = {'\0'};
    snprintf(cmdbuf, 1024, "objdump -d \"%s\" | grep \"%s\"", filename, REGEXP_STRING);
    g_objdump_num += 1;
    g_grep_num += 1;
    FILE *fin = popen(cmdbuf, "r");
    if(fin == NULL) return -3;
    char line[128] = {'\0'};
    while (fgets(line, sizeof(line), fin))
    {
        HACK_DEBUG(0, "out=%p and line=\"%s\"\n", out, line);
        ret = 1;
        if (out) *out = strdup(line);
        break;
    }
    fclose(fin);
    return ret;
}

/*
 * 功  能： 检查可执行文件是否存在，如果存在则进一步做反汇编检测
 * 参  数： filename, 可执行文件的绝对路径
 *          out, 存储反汇编检测到指令信息
 * 返回值： -1，非法的文件名称
 *          -2，文件不存在
 *          -3, 文件解析失败
 *          -4, 这条命令不解析
 *           0，可执行文件不存在in指令调用
 *           1，可执行文件存在in指令的调用
 */
int executable_file_check(const char *filename, char **out)
{
    char *pos = NULL;
    if (NULL == filename || '\0' == *filename)
        return -1;
    
    if (0 != access(filename, R_OK))
        return -2;

    /* 由于使用objdump和grep两条命令来过滤in指令，如果不加以处理会导致无限循环 */
    pos = strstr(filename, "objdump");
    if (pos && strlen(pos) == 7) 
    {
        g_objdump_num -= 1;
        return -4;
    }
    pos = strstr(filename, "grep");
    if (pos && strlen(pos) == 4)
    {
        g_grep_num -= 1;
        return -4;
    }
    return disassembl_check_in(filename, out);
}
