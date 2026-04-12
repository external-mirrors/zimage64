#include "cmdline.h"
#include "udt/udt-static.h"

static int strcmp(const char* a, const char* b)
{
    while(*a && *a == *b)
    {
        a++;
        b++;
    }
    return (int)*a - (int)*b;
}

static void strcpy(char* a, const char* b)
{
    while((*a++ = *b++));
}

void maybe_replace_cmdline(void* dt)
{
    struct udt udt;
    if(udt_open(&udt, dt, -1))
        return;
    struct udt_node root;
    udt_begin(&root, &udt);
    {
        struct udt_node subroot = root;
        if(!udt_next(&subroot, UDT_CHILD) && udt_next(&subroot, UDT_SIBLING) && !*udt_node_name(&subroot))
            root = subroot;
    }
    struct udt_node chosen = root;
    if(udt_next(&chosen, UDT_CHILD))
        return;
    while(strcmp(udt_node_name(&chosen), "chosen"))
        if(udt_next(&chosen, UDT_SIBLING))
            return;
    struct udt_property cmdline_override;
    if(udt_first_property(&chosen, &cmdline_override))
        return;
    while(strcmp(udt_property_name(&cmdline_override), "zimage64,cmdline-override"))
        if(udt_next_property(&cmdline_override))
            return;
    struct udt_property prop;
    udt_first_property(&chosen, &prop);
    for(;;)
    {
        struct udt_property next = prop;
        int last = udt_next_property(&next);
        if(!strcmp(udt_property_name(&prop), "bootargs"))
            udt_remove_property(&prop);
        if(last)
            break;
        prop = next;
    }
    strcpy((char*)udt_property_name(&cmdline_override), "bootargs");
    return;
}
