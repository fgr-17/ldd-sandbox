struct platform_data {
    int size;
    int perm;
    const char* serial_number;
};

struct dev_custom_config {
    int config_1;
    int config_2;
};

enum pdev_names {
    PDEV_A1X,
    PDEV_B1X
};

struct dev_custom_config pdev_config_table[] = {
    [PDEV_A1X] = {.config_1 = 10, .config_2 = 20},
    [PDEV_B1X] = {.config_1 = 100, .config_2 = 200}
};
