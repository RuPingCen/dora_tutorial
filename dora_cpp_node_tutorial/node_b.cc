extern "C" // 用 C 语言的符号规则去链接
{
#include "node_api.h"
}
// #include "node_api.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <string>
#include <iostream>

int run(void *dora_context)
{
    bool to_exit_process = false;

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

            if (id == "data")
            {
                char *data;
                size_t data_len;
                read_dora_input_data(event, &data, &data_len);

                char buffer[256];
                size_t len = (data_len < 255) ? data_len : 255;
                memcpy(buffer, data, len);
                buffer[len] = '\0';

                time_t now = time(NULL);
                char *time_str = ctime(&now);
                if (time_str)
                {
                    time_str[strlen(time_str) - 1] = '\0'; // Remove newline
                }

                std::cout << "[Node C] Received: " << buffer << " | Time: " << time_str << std::endl;
            }
        }
        else if (ty == DoraEventType_Stop)
        {
            std::cout << "Received stop event" << std::endl;
            free_dora_event(event);
            break;
        }

        free_dora_event(event);
    }

    free_dora_context(dora_context);

    return 0;
}

int main()
{
    std::cout << "HELLO node c (using C API)" << std::endl;

    auto dora_context = init_dora_context_from_env();
    if (dora_context == NULL)
    {
        std::cerr << "failed to init dora context" << std::endl;
        return 1;
    }
    auto ret = run(dora_context);
    free_dora_context(dora_context);

    std::cout << "node c exit " << std::endl;

    return ret;
}