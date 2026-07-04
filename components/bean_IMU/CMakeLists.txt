set(priv_requires "bean_system" "esp_driver_i2c" "freertos" "esp_rom")
idf_component_register(SRCS "bean_imu.c" "BMI08X/bmi08xa.c" "BMI08X/bmi08g.c" "BMI08X/bmi08a.c" "BMI08X/bmi088_mma.c"
                    INCLUDE_DIRS "include" "BMI08X"
                    PRIV_REQUIRES ${priv_requires})