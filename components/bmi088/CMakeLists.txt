set(priv_requires "driver" "freertos" "esp_rom")
idf_component_register(SRCS "bmi088.c" "BMI08X/bmi08xa.c" "BMI08X/bmi08g.c" "BMI08X/bmi08a.c" "BMI08X/bmi088_mma.c"
                    INCLUDE_DIRS "include" "BMI08X"
                    PRIV_REQUIRES ${priv_requires})