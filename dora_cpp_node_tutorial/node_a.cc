extern "C" // 用 C 语言的符号规则去链接
{
#include "node_api.h"
}
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <string>
#include <iostream>

int main()
{
    bool to_exit_process = false;

    void *dora_context = init_dora_context_from_env();
    if (dora_context == NULL)
    {
        std::cerr << "failed to init dora context" << std::endl;
        return -1;
    }

    const char *out_id = "data";
    const char *message = "Hello from Node A (C API)!";

    std::cout << "Node A started, sending: " << message << std::endl;

    while (!to_exit_process)
    {
        void *event = dora_next_event(dora_context);
        if (event == NULL)
        {
            std::cerr << "[c node] ERROR: unexpected end of event" << std::endl;
            return -1;
        }

        enum DoraEventType ty = read_dora_event_type(event);

        if (ty == DoraEventType_Input)
        {
            char *id_ptr;
            size_t id_len;
            read_dora_input_id(event, &id_ptr, &id_len);
            std::string id(id_ptr, id_len);

            // char* data;
            // size_t data_len;
            // read_dora_input_data(event, &data, &data_len);

            if (id == "tick")
            {
                // send string to node B
                int result = dora_send_output(dora_context, (char *)out_id, strlen(out_id), (char *)message, strlen(message));
                if (result != 0)
                {
                    std::cerr << "failed to send output: " << result << std::endl;
                }
            }
            free_dora_event(event);
        }
        else if (ty == DoraEventType_Stop)
        {
            std::cout << "Received stop event" << std::endl;
            free_dora_event(event);
            to_exit_process = true;
            break;
        }
    }

    free_dora_context(dora_context);
    return 0;
}