set(srcs "main.c"
         "uart/zb_uart.c")

set(include_dir "."
                "uart")


idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dir})