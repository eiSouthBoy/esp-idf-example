# 获取项目路径，并赋值给变量
idf_build_get_property(project_dir PROJECT_DIR)
# 嵌入二进制数据或文本文件，EMBED_FILES：二进制文件标志，EMBED_TXTFILES：文本文件标志
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)