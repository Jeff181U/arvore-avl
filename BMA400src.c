/* src/main.c */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

// Pega o nosso dispositivo do DeviceTree usando o label que definimos no overlay.
// DT_NODELABEL("BMA400") encontra o nó com label = "BMA400"
#define BMA400_NODE DT_NODELABEL(BMA400)
static const struct device *bma400_dev = DEVICE_DT_GET(BMA400_NODE);

// Função para converter o valor do sensor (struct sensor_value) para double
static double sensor_value_to_double(const struct sensor_value *val)
{
    return (double)val->val1 + (double)val->val2 / 1000000.0;
}

int main(void)
{
    // Verifica se o dispositivo foi encontrado e está pronto para uso
    if (!device_is_ready(bma400_dev)) {
        printk("Erro: Sensor BMA400 não encontrado ou não está pronto!\n");
        return 0;
    }

    printk("Sensor BMA400 encontrado! Lendo dados...\n");

    while (1) {
        struct sensor_value accel_x, accel_y, accel_z;

        // 1. Busca uma nova amostra de dados do sensor
        if (sensor_sample_fetch(bma400_dev) < 0) {
            printk("Falha ao buscar amostra do sensor.\n");
            k_sleep(K_MSEC(1000));
            continue;
        }

        // 2. Obtém os dados de aceleração para cada canal (eixo)
        sensor_channel_get(bma400_dev, SENSOR_CHAN_ACCEL_X, &accel_x);
        sensor_channel_get(bma400_dev, SENSOR_CHAN_ACCEL_Y, &accel_y);
        sensor_channel_get(bma400_dev, SENSOR_CHAN_ACCEL_Z, &accel_z);

        // 3. Imprime os valores no console
        printf("Aceleração: X=%.2f, Y=%.2f, Z=%.2f (m/s^2)\n",
               sensor_value_to_double(&accel_x),
               sensor_value_to_double(&accel_y),
               sensor_value_to_double(&accel_z));

        // Espera 2 segundos antes da próxima leitura
        k_sleep(K_MSEC(2000));
    }
    return 0;
}
