
extern int __flash_binary_start, __logical_binary_start;
#define PROG_AREA_BEGIN ((uintptr_t)&__flash_binary_start)
#define PROG_AREA_END ((uintptr_t)&__logical_binary_start)
#define PROG_AREA_SIZE (PROG_AREA_END - PROG_AREA_BEGIN)

typedef struct
{
    uintptr_t prog_addr;
    uint32_t size;
    char filename[80];
} prog_info_t;

volatile prog_info_t const *get_prog_info(void);

void clear_prog_info(void);
void set_prog_info(uint32_t prog_addr, uint32_t prog_size, const char *filename);
bool check_prog_info(void);
