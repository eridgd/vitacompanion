#include "input.h"

#include <vitacompanion_kernel.h>

#include <stdbool.h>

static bool input_ready;

int input_start(void)
{
    if (vitaCompanionKernelGetApiVersion() !=
        VITACOMPANION_KERNEL_ABI_VERSION)
        return -1;

    input_ready = true;
    return 0;
}

void input_end(void)
{
    if (input_ready)
        vitaCompanionKernelReset();
    input_ready = false;
}

bool input_is_ready(void)
{
    return input_ready;
}

int input_apply(const vitacompanion_input_action *action)
{
    if (!input_ready || !action)
        return -1;

    switch (action->type)
    {
    case VITACOMPANION_INPUT_BUTTON:
        return vitaCompanionKernelSetButtons(
            action->data.button.mask, action->active);
    case VITACOMPANION_INPUT_ANALOG:
        return vitaCompanionKernelSetAnalog(
            action->data.analog.stick,
            action->data.analog.x,
            action->data.analog.y,
            action->active);
    case VITACOMPANION_INPUT_TOUCH:
        return vitaCompanionKernelSetTouch(
            action->data.touch.port,
            action->data.touch.slot |
                (action->active ? VITACOMPANION_TOUCH_ACTIVE_FLAG : 0),
            action->data.touch.x,
            action->data.touch.y);
    case VITACOMPANION_INPUT_RESET:
        return vitaCompanionKernelReset();
    default:
        return -1;
    }
}
