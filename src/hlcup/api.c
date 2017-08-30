#include "api.h"

#include "core.h"
#include "log.h"
#include <time.h>
#include <dirent.h>

#include "objects.h"

#include <stdbool.h>
#include <stdio.h>

#include <assert.h>

#if 0

#define EBUSY 1
typedef unsigned spinlock;

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

static inline unsigned xchg_32(void *ptr, unsigned x)
{
    __asm__ __volatile__("xchgl %0,%1"
            :"=r" ((unsigned) x)
            :"m" (*(volatile unsigned *)ptr), "0" (x)
            :"memory");

    return x;
}


static void spin_lock(spinlock *lock)
{
    while (1) {
        if (!xchg_32(lock, EBUSY))
            return;

        while (*lock)
            cpu_relax();
    }
}

static void spin_unlock(spinlock *lock)
{
    barrier();
    *lock = 0;
}

/* static int spin_trylock(spinlock *lock) */
/* { */
/*     return xchg_32(lock, EBUSY); */
/* } */

spinlock user_lock;
spinlock location_lock;
spinlock visit_lock;
spinlock visits_hash_lock;

#define LOCK(NAME) spin_lock(&(NAME##_lock));
#define UNLOCK(NAME) spin_unlock(&(NAME##_lock));
#else

#define LOCK(a)
#define UNLOCK(a)
#endif

uint64_t real_now = 0;

static int32_t get_bday(uint32_t age);

/* HACK:
 * On insert want duplicates by visited_at (so return -1 even if elements are equal)
 * But in case of delete we need strict compare
 */
static __thread bool tree_compare_eq = false;
int
my_tree_compare(visit_t *a, visit_t *b)
{
    if (a->visited_at < b->visited_at)
        return -1;
    if (a->visited_at > b->visited_at)
        return 1;
    return tree_compare_eq ? 0 : -1;
}

void*
my_malloc(void *ctx)
{
    (void)ctx;
    return malloc(BPS_TREE_EXTENT_SIZE);
    /* return memalloc_alloc(BPS_TREE_EXTENT_SIZE); */
}
void
my_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
    /* memalloc_free(ptr); */
}

#define GEN_HELPERS(TYPE)                                                                                  \
    static TYPE##_t **TYPE##s_array;                                                                       \
    static uint32_t TYPE##s_array_len;                                                                     \
    static tommy_hashdyn TYPE##s_hash;                                                                     \
    static uint32_t TYPE##s_cnt = 0;                                                                       \
    int TYPE##_hash_compare(const void* arg, const void* obj) {                                            \
        return *(const uint32_t*)arg != ((const TYPE##_t *)obj)->id;                                       \
    }                                                                                                      \
    TYPE##_t* TYPE##s_hash_get(uint32_t id) {                                                              \
        if (id < TYPE##s_array_len) {                                                                      \
            return TYPE##s_array[id];                                                                      \
        } else {                                                                                           \
            return tommy_hashdyn_search(&(TYPE##s_hash), TYPE##_hash_compare, &id, tommy_inthash_u32(id)); \
        }                                                                                                  \
    }                                                                                                      \
    void TYPE##s_hash_insert(TYPE##_t *obj) {                                                              \
        TYPE##s_cnt++;                                                                                     \
        LOCK(TYPE);                                                                                        \
        if (obj->id < TYPE##s_array_len) {                                                                 \
            TYPE##s_array[obj->id] = obj;                                                                  \
        } else {                                                                                           \
            tommy_hashdyn_insert(&(TYPE##s_hash), &obj->node, obj, tommy_inthash_u32(obj->id));            \
        }                                                                                                  \
        UNLOCK(TYPE);                                                                                      \
    }

GEN_HELPERS(user)
GEN_HELPERS(location)
GEN_HELPERS(visit)

#undef GEN_HELPERS


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#define LOAD_FROM_FILE(TYPE, FILEPATH) do {                              \
    struct stat statbuf;                                                 \
    int fd = open(FILEPATH, O_RDONLY);                                   \
    fstat(fd, &statbuf);                                                 \
    void *src = mmap(0, statbuf.st_size, PROT_WRITE | PROT_READ, MAP_PRIVATE, fd, 0); \
    const char *e = src + statbuf.st_size; \
    char *p = strchr(src, '[') + 1;                                      \
    while (1) {                \
        TYPE##s_new(&p, e);   \
        if (p == NULL) { \
            printf("load error\n"); \
                break;         \
            }                  \
        if (p[1] == ']')       \
            break;                                                       \
        p+=3; \
        /*printf("%d\n", p-q);*/ \
    }                                                                    \
    munmap(src, statbuf.st_size);                                        \
    close(fd);                                                           \
} while(0);



void
api_init()
{
    users_array = NULL;
    locations_array = NULL;
    visits_array = NULL;

    users_array_len = 1010000;
    locations_array_len = 710000;
    visits_array_len = 10010000;

    locations_array = (location_t**)calloc(locations_array_len, sizeof(location_t*));
    users_array = (user_t**)calloc(users_array_len, sizeof(user_t*));
    visits_array = (visit_t**)calloc(visits_array_len, sizeof(visit_t*));

    if (!users_array)
        users_array_len = 0;
    if (!locations_array)
        locations_array_len = 0;
    if (!visits_array)
        visits_array_len = 0;

    /* printf("visit: =%lu= %lu Mb=\n", sizeof(visit_t), sizeof(visit_t)*10000000/1024/1024); */
    /* printf("user : =%lu= %lu Mb=\n", sizeof(user_t), sizeof(user_t)*1000074/1024/1024); */
    /* printf("loca :=%lu= %lu Mb=\n", sizeof(location_t), sizeof(location_t)*761314/1024/1024); */
    /* exit(0); */
    tommy_hashdyn_init(&users_hash);
    tommy_hashdyn_init(&locations_hash);
    tommy_hashdyn_init(&visits_hash);

#if 1
    DIR *dir = opendir("./data/");
    if (!dir) {
        fprintf(stderr, "can't open ./data\n");
        exit(1);
    }
    struct dirent *dp;
    while ((dp=readdir(dir)) != NULL) {
        if (dp->d_name[0] == '.')
            continue;
        char *fn = dp->d_name;
        char fullpath[280];
        snprintf(fullpath, sizeof(fullpath), "./data/%s", fn);

        switch(fn[0]) {
        case 'u': //users_
            LOAD_FROM_FILE(user, fullpath);
            break;
        case 'l': //locations_
            LOAD_FROM_FILE(location, fullpath);
            break;
        }
    }
    closedir(dir);

    do {
        FILE *fh = fopen("/tmp/data/options.txt", "r");
        if (!fh)
            break;
        fscanf(fh, "%lu", &real_now);
        fclose(fh);
        fprintf(stderr, "now = %lu\n", real_now);
    } while(0);

    dir = opendir("./data/");
    while ((dp=readdir(dir)) != NULL) {
        if (dp->d_name[0] == '.')
            continue;
        char *fn = dp->d_name;
        char fullpath[280];
        snprintf(fullpath, sizeof(fullpath), "./data/%s", fn);
        if (fn[0] == 'v') { //visits_
            LOAD_FROM_FILE(visit, fullpath);
        }
    }
    closedir(dir);
#endif

    api_show_cnt();
}

void
api_show_cnt()
{
    fprintf(stderr, "users: %u | locations: %u | visits: %u\n", users_cnt, locations_cnt, visits_cnt);
}

#define BASIC_UPDATE(TYPE)                                         \
    TYPE##_t *obj;                                                 \
    LOCK(TYPE);                                                    \
    obj = TYPE##s_hash_get(id);                                    \
    if (obj == NULL) {                                             \
        UNLOCK(TYPE);                                              \
        return 404;                                                \
    }                                                              \
    TYPE##_t old;                                                  \
    memcpy(&old, obj, sizeof(old));                                \
    if (TYPE##_parse(obj, data, e) == NULL || obj->id != old.id) { \
        /* rollback update */                                      \
        memcpy(obj, &old, sizeof(old));                            \
        UNLOCK(TYPE);                                              \
        return 400;                                                \
    }                                                              \
    UNLOCK(TYPE);

int
users_new(char **data, const char *e)
{
    user_t *obj = (user_t*)calloc(1, sizeof(user_t));
    char *ptr = *data;
    ptr = user_parse(obj, ptr, e);
    *data = ptr;
    if (ptr == NULL)
        return 400;

    users_hash_insert(obj);

    user_update_json(obj);

    visits_tree_create(&obj->vtree, 0, my_malloc, my_free, NULL);
    return 200;
}

user_t*
users_get(uint32_t id)
{
    return users_hash_get(id);
}

int
users_update(uint32_t id, char *data, const char *e)
{
    BASIC_UPDATE(user);
    user_update_json(obj);
    return 200;
}

//

int
locations_new(char **data, const char *e)
{
    /* const char *begin = *data; */
    location_t *obj = (location_t*)calloc(1, sizeof(location_t));
    /* obj->simple_avg_cnt = 0; */
    /* obj->simple_avg_sum = 0; */
    char *ptr = *data;
    ptr = location_parse(obj, ptr, e);
    *data = ptr;
    if (ptr == NULL)
        return 400;

    locations_hash_insert(obj);

    location_update_json(obj);
    /* obj->len_json = *data - begin + 1#<{(|}|)}>#; */
    /* memcpy(obj->json, begin, obj->len_json); */

    visits_tree_create(&obj->vtree, 0, my_malloc, my_free, NULL);
    return 200;
}

location_t*
locations_get(uint32_t id)
{
    return locations_hash_get(id);
}
int
locations_update(uint32_t id, char *data, const char *e)
{
    BASIC_UPDATE(location);

    location_update_json(obj);

    /* struct visits_tree *vtree = &obj->vtree; */
    /* struct visits_tree_iterator it = visits_tree_iterator_first(vtree); */
    /* if (visits_tree_iterator_is_invalid(&it)) */
    /*     return 200; */
    /* do { */
    /*     visit_t *v = *visits_tree_iterator_get_elem(vtree, &it); */
    /*     visit_update_uv_json(v); */
    /* } while (visits_tree_iterator_next(vtree, &it)); */

    return 200;
}


//

static void
add_visit_by_user(visit_t *obj, user_t *u)
{
    /* if (u == NULL) */
    /*     return; */

    visits_tree_insert(&u->vtree, obj, NULL);
}

static void
del_visit_by_user(visit_t *obj, user_t *u)
{
    /* if (u == NULL) */
    /*     return; */

    tree_compare_eq = true;
    visits_tree_delete(&u->vtree, obj);
    tree_compare_eq = false;
}

static void
add_visit_by_location(visit_t *obj, location_t *l)
{

    /* if (l == NULL) */
    /*     return; */

    visits_tree_insert(&l->vtree, obj, NULL);
    l->simple_avg_cnt++;
    l->simple_avg_sum += obj->mark;
}

static void
del_visit_by_location(visit_t *obj, location_t *l)
{
    /* if (l == NULL) */
    /*     return; */

    tree_compare_eq = true;
    visits_tree_delete(&l->vtree, obj);
    tree_compare_eq = false;

    l->simple_avg_cnt--;
    l->simple_avg_sum -= obj->mark;
}




int
visits_new(char **data, const char *e)
{
    /* const char *begin = *data; */
    visit_t *obj = (visit_t*)calloc(1, sizeof(visit_t));

    char *ptr = *data;
    ptr = visit_parse(obj, ptr, e);
    *data = ptr;
    if (ptr == NULL)
        goto _400;

    user_t *user = users_hash_get(obj->user);
    if (user == NULL)
        goto _400;
    obj->user_obj = user;

    location_t *loc = locations_hash_get(obj->location);
    if (loc == NULL)
        goto _400;
    obj->location_obj = loc;

    visit_update_json(obj);
    /* visit_update_uv_json(obj); */

    visits_hash_insert(obj);

    LOCK(visits_hash);
    add_visit_by_user(obj, obj->user_obj);
    add_visit_by_location(obj, obj->location_obj);
    UNLOCK(visits_hash);

    return 200;
_400:
    free(obj);
    return 400;
}

visit_t*
visits_get(uint32_t id)
{
    return visits_hash_get(id);
}

int
visits_update(uint32_t id, char *data, const char *e)
{
    // json parser destroys original data, and we call it twice here,
    // so just save original state.
    // TODO: remove this hack

    char xxxbuf[4096];
    memcpy(xxxbuf, data, e - data);
    char *xxxbuf_e = xxxbuf + (e - data);


    /* printf("req to update: |%.*s|\n",  e - data, data); */
    visit_t *obj;

    LOCK(visit);
    obj = visits_hash_get(id);
    UNLOCK(visit);
    if (obj == NULL)
        return 404;

    //I have a bug here and no time to find & fix it, so always update trees
#if 0
    bool need_update_by_user = true;
    bool need_update_by_location = true;

    uint64_t old_user = obj->user;
    uint64_t old_location = obj->location;
    uint64_t old_visited_at = obj->visited_at;
    location_t *old_location_obj = obj->location_obj;
    user_t *old_user_obj = obj->user_obj;

    if (visit_parse(obj, data, e) == NULL)
        return 400;

    if (obj->visited_at != old_visited_at) {
        need_update_by_user = true;
        need_update_by_location = true;
    } else {
        if (obj->user != old_user)
            need_update_by_user = true;
        if (obj->location != old_location)
            need_update_by_location = true;
    }

    uint64_t new_visited_at = obj->visited_at;
    if (need_update_by_user) {
        user_t *user = users_hash_get(obj->user);
        obj->user_obj = user;

        obj->visited_at = old_visited_at;
        del_visit_by_user(obj, old_user_obj);
        obj->visited_at = new_visited_at;


        add_visit_by_user(obj, user);
    }

    if (need_update_by_location) {
        location_t *loc = locations_hash_get(obj->location);
        obj->location_obj = loc;

        obj->visited_at = old_visited_at;
        del_visit_by_location(obj, old_location_obj);
        obj->visited_at = new_visited_at;

        add_visit_by_location(obj, loc);
    }

    visit_update_json(obj);


#else
    //buggy:
    visit_t tmp;
    memset(&tmp, 0, sizeof(visit_t));

    //TODO: handle update id?
    /* printf("tmp: (%p - %p)\n", data, e); */
    if (visit_parse(&tmp, data, e) == NULL) // || obj->id != tmp.id)
        return 400;

    /* visit_print(&tmp); */
    bool need_update_by_user = false;
    bool need_update_by_location = false;

    if (tmp.visited_at != obj->visited_at) {
        need_update_by_user = true;
        need_update_by_location = true;
    } else {
        if (tmp.user != obj->user)
            need_update_by_user = true;

        if (tmp.location != obj->location)
            need_update_by_location = true;
    }

    tmp.user_obj = NULL;
    if (tmp.user != obj->user) {
        user_t *user = users_hash_get(tmp.user);
        /* if (user == NULL) */
        /*     return 400; */
        tmp.user_obj = user;
    }

    tmp.location_obj = NULL;
    if (tmp.location != obj->location) {
        location_t *loc = locations_hash_get(tmp.location);
        /* if (loc == NULL) */
        /*     return 400; */
        tmp.location_obj = loc;
    }

    /* LOCK(visits_hash); */

    if (need_update_by_user)
        del_visit_by_user(obj, obj->user_obj);

    if (need_update_by_location)
        del_visit_by_location(obj, obj->location_obj);

    //real object update
    //100% without errors
    /* LOCK(visit); */

    visit_parse(obj, xxxbuf, xxxbuf_e);
    /* char *q = visit_parse(obj, data, e); */
    /* printf("obj post real q=%p: (%p - %p)\n", q, data, e); */

    if (tmp.user_obj)
        obj->user_obj = tmp.user_obj;

    if (tmp.location_obj)
        obj->location_obj = tmp.location_obj;

    visit_update_json(obj);
    /* visit_update_uv_json(obj); */

    /* UNLOCK(visit); */

    if (need_update_by_user)
        add_visit_by_user(obj, obj->user_obj);

    if (need_update_by_location)
        add_visit_by_location(obj, obj->location_obj);

    /* UNLOCK(visits_hash); */
#endif

    return 200;
}


int
users_visits(uint32_t userid, uint32_t from_date, uint32_t to_date, const char *country, uint32_t to_distance, char **out)
{
    user_t *u = users_hash_get(userid);
    if (u == NULL)
        return 404;

    struct visits_tree *vtree = &u->vtree;
    if (visits_tree_size(vtree) == 0)
        return 200;

    struct visits_tree_iterator it = visits_tree_lower_bound(vtree, from_date, NULL);
    if (visits_tree_iterator_is_invalid(&it))
        return 200;

    int cnt = 0;
    int off = 0;
    char *p = *out;
    do {
        visit_t *v = *visits_tree_iterator_get_elem(vtree, &it);
        if (v->visited_at >= to_date)
            break;

        location_t *loc = v->location_obj;
        /* printf("Try %d | visit_id:%d country:|%s|\n", v->visited_at, v->id, loc->country); */

        if (loc->distance < to_distance && (country == NULL || strcmp(country, loc->country) == 0)) {
            /* APPEND_BUF(p, off, v->uv_json, v->uv_len_json); */

            APPEND_CONST_STRING(p, off, "{\"mark\": ");
            APPEND_INT(p, off, v->mark);
            APPEND_CONST_STRING(p, off, ",\"visited_at\": ");
            APPEND_INT(p, off, v->visited_at);
            APPEND_CONST_STRING(p, off, ",\"place\": \"");
            APPEND_BUF(p, off, loc->place, loc->len_place);
            APPEND_CONST_STRING(p, off, "\"},");
            cnt++;
        }

    } while (visits_tree_iterator_next(vtree, &it));
    //remove last ','
    if (cnt)
        off--;

    *out += off;
    return 200;
}


/* -106704000 */
static int32_t
get_bday(uint32_t age)
{
    static __thread uint64_t now = 0;
    static __thread struct tm t;
    if (now == 0) {
        if (real_now == 0) {
            fprintf(stderr, "using time() now\n");
            /* now = ev_now(gthread_loop); */
            now = time(NULL);
        } else {
            fprintf(stderr, "using file now\n");
            now = real_now;
        }
        time_t a = now;
        gmtime_r(&a, &t);
        t.tm_sec = 0;
        t.tm_min = 0;
        t.tm_hour = 0;
    }

    /* if (age == 0) */
    /*     return now; */
    /* if (age == UINT32_MAX) */
    /*     return INT32_MIN; */

    t.tm_year -= age;
    int32_t res = timegm(&t);
    t.tm_year += age;
    return res;
}


int
locations_avg(uint32_t locationid, uint32_t from_date, uint32_t to_date, uint32_t from_age, uint32_t to_age, char gender, char *result)
{
    location_t *loc = locations_hash_get(locationid);
    if (loc == NULL)
        return 404;

    struct visits_tree *vtree = &loc->vtree;
    if (visits_tree_size(vtree) == 0) {
        *result = '0';
        return 200;
    }

    if (from_date == 0 && to_date == UINT32_MAX && from_age == 0 && to_age == UINT32_MAX && gender == '\0') {
        float2str(loc->simple_avg_sum, loc->simple_avg_cnt, result);
        return 200;
    }

    //not a bug, reversed!
    int32_t from_birthday, to_birthday;

    if (to_age != UINT32_MAX) {
        from_birthday = get_bday(to_age);
    } else {
        from_birthday = INT32_MIN;
    }
    if (from_age != 0) {
        to_birthday = get_bday(from_age);
    } else {
        to_birthday = INT32_MAX;
    }

    struct visits_tree_iterator it = visits_tree_lower_bound(vtree, from_date, NULL);
    if (visits_tree_iterator_is_invalid(&it)) {
        *result = '0';
        return 200;
    }

    double avg = 0;
    int cnt = 0;
    do {
        visit_t *v = *visits_tree_iterator_get_elem(vtree, &it);
        if (v->visited_at >= to_date)
            break;
        user_t *u = v->user_obj;
        /* printf("try visit %d mark %d | %u < %u && %u < %u\n", v->id, v->mark, from_birthday, u->birth_date, u->birth_date, to_birthday); */
        if (from_birthday <= u->birth_date && u->birth_date < to_birthday && (gender == '\0' || gender == u->gender)) {
            /* printf("visit %d mark %d\n", v->id, v->mark); */
            avg += v->mark;
            cnt++;
        }
    } while (visits_tree_iterator_next(vtree, &it));

    if (avg == 0) {
        *result = '0';
    } else {
        float2str(avg, cnt, result);
    }

    return 200;
}
