
typedef void interrupt (*interrupt_handler_t)(void);

struct interrupt_hook
{
    int interrupt_num;
    interrupt_handler_t old_isr;
    int force_vector;
};

int FindAndHookInterrupt(struct interrupt_hook *state,
                         interrupt_handler_t isr);
void RestoreInterrupt(struct interrupt_hook *state);

