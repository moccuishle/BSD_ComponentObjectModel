/*
 * registry.c - 类注册表
 *
 * 对应 Windows 注册表中的 HKCR\CLSID\{...}\InprocServer32。
 * 这里用一份纯文本文件代替注册表数据库，格式：
 *
 *   # 注释
 *   {CLSID}  <模块路径>  [描述...]
 *
 * 空行与以 '#' 开头的行会被忽略。描述是行尾剩余部分，可含空格。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "bsdcom.h"
#include "internal.h"

#define REG_LINE_MAX 1024

static BsdComClassEntry *g_head = NULL;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static void free_entry(BsdComClassEntry *e)
{
    if (!e) return;
    free(e->module);
    free(e->desc);
    free(e);
}

BsdComClassEntry *BsdComRegistryFind(BSDCOM_REFCLSID clsid)
{
    BsdComClassEntry *e;
    if (!clsid) return NULL;
    for (e = g_head; e; e = e->next) {
        if (BsdComGuidEqual(&e->clsid, clsid)) return e;
    }
    return NULL;
}

void BsdComRegistryForEach(void (*fn)(BsdComClassEntry *entry))
{
    BsdComClassEntry *e;
    if (!fn) return;
    pthread_mutex_lock(&g_lock);
    for (e = g_head; e; e = e->next) fn(e);
    pthread_mutex_unlock(&g_lock);
}

BSDCOM_HRESULT BsdComRegisterClass(BSDCOM_REFCLSID clsid, const char *module_path,
                                   const char *description)
{
    BsdComClassEntry *e;
    char *mod, *desc;

    if (!clsid || !module_path) return BSDCOM_E_POINTER;

    mod = strdup(module_path);
    desc = strdup(description ? description : "");
    if (!mod || !desc) {
        free(mod);
        free(desc);
        return BSDCOM_E_OUTOFMEMORY;
    }

    pthread_mutex_lock(&g_lock);
    e = BsdComRegistryFind(clsid);
    if (e) {
        /* 重复注册：覆盖旧条目，符合“后写覆盖”的直觉 */
        free(e->module);
        free(e->desc);
        e->module = mod;
        e->desc = desc;
        pthread_mutex_unlock(&g_lock);
        return BSDCOM_S_OK;
    }

    e = (BsdComClassEntry *)calloc(1, sizeof(*e));
    if (!e) {
        pthread_mutex_unlock(&g_lock);
        free(mod);
        free(desc);
        return BSDCOM_E_OUTOFMEMORY;
    }
    e->clsid = *clsid;
    e->module = mod;
    e->desc = desc;
    e->dlhandle = NULL;
    e->next = g_head;
    g_head = e;
    pthread_mutex_unlock(&g_lock);
    return BSDCOM_S_OK;
}

BSDCOM_HRESULT BsdComUnregisterClass(BSDCOM_REFCLSID clsid)
{
    BsdComClassEntry **pp, *victim = NULL;

    if (!clsid) return BSDCOM_E_POINTER;

    pthread_mutex_lock(&g_lock);
    for (pp = &g_head; *pp; pp = &(*pp)->next) {
        if (BsdComGuidEqual(&(*pp)->clsid, clsid)) {
            victim = *pp;
            *pp = victim->next;
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);

    if (!victim) return BSDCOM_REGDB_E_CLASSNOTREGISTERED;
    BsdComLoaderUnload(victim);
    free_entry(victim);
    return BSDCOM_S_OK;
}

BSDCOM_HRESULT BsdComRegistryLoadFile(const char *path)
{
    FILE *fp;
    char line[REG_LINE_MAX];
    int lineno = 0;

    if (!path) return BSDCOM_E_POINTER;
    fp = fopen(path, "r");
    if (!fp) return BSDCOM_E_FAIL;

    while (fgets(line, sizeof(line), fp)) {
        char *p = line, *clsid_text, *module, *desc = NULL;
        BSDCOM_CLSID clsid;

        lineno++;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '#') continue;

        /* 切出 CLSID */
        clsid_text = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        if (!*p) {
            fprintf(stderr, "bsdcom: %s:%d 缺少模块路径\n", path, lineno);
            continue;
        }
        *p++ = '\0';

        while (*p == ' ' || *p == '\t') p++;
        module = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        if (*p) {
            *p++ = '\0';
            while (*p == ' ' || *p == '\t') p++;
            desc = p;
            {   /* 去掉行尾空白 */
                size_t n = strlen(desc);
                while (n > 0 && (desc[n - 1] == '\n' || desc[n - 1] == '\r' ||
                                 desc[n - 1] == ' '  || desc[n - 1] == '\t')) {
                    desc[--n] = '\0';
                }
            }
        }

        if (BsdComGuidFromString(clsid_text, &clsid) < 0) {
            fprintf(stderr, "bsdcom: %s:%d CLSID 格式错误: %s\n", path, lineno, clsid_text);
            continue;
        }
        if (*module == '\0') {
            fprintf(stderr, "bsdcom: %s:%d 模块路径为空\n", path, lineno);
            continue;
        }
        BsdComRegisterClass(&clsid, module, desc ? desc : "");
    }
    fclose(fp);
    return BSDCOM_S_OK;
}

BSDCOM_HRESULT BsdComRegistryLoadDefault(void)
{
    static const char *candidates[] = {
        "/etc/bsdcom.reg",
        "/usr/local/etc/bsdcom.reg",
        "./bsdcom.reg"
    };
    const char *env = getenv("BSDCOM_REGISTRY");
    size_t i;

    if (env && *env) {
        if (BSDCOM_SUCCEEDED(BsdComRegistryLoadFile(env))) return BSDCOM_S_OK;
        fprintf(stderr, "bsdcom: 无法加载 BSDCOM_REGISTRY=%s\n", env);
    }
    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (BSDCOM_SUCCEEDED(BsdComRegistryLoadFile(candidates[i]))) {
            return BSDCOM_S_OK;
        }
    }
    fprintf(stderr, "bsdcom: 未找到类注册表，组件激活将全部失败\n");
    return BSDCOM_E_FAIL;
}

void BsdComRegistryDump(void)
{
    BsdComClassEntry *e;
    char text[40];

    pthread_mutex_lock(&g_lock);
    for (e = g_head; e; e = e->next) {
        BsdComGuidToString(&e->clsid, text);
        printf("%s  %-40s  %s\n", text, e->module, e->desc ? e->desc : "");
    }
    pthread_mutex_unlock(&g_lock);
}
