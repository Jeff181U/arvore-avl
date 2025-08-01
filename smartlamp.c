#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/leds.h>

MODULE_AUTHOR("DevTITANS <devtitans@icomp.ufam.edu.br>");
MODULE_DESCRIPTION("Driver de acesso ao SmartLamp (ESP32 com Chip Serial CP2102");
MODULE_LICENSE("GPL");

#define MAX_RECV_LINE 100 // Tamanho máximo de uma linha de resposta do dispositvo USB

static char recv_line[MAX_RECV_LINE];              // Armazena dados vindos da USB até receber um caractere de nova linha '\n'
static struct usb_device *smartlamp_device;        // Referência para o dispositivo USB
static uint usb_in, usb_out;                       // Endereços das portas de entrada e saida da USB
static char *usb_in_buffer, *usb_out_buffer;       // Buffers de entrada e saída da USB
static int usb_max_size;                           // Tamanho máximo de uma mensagem USB

#define VENDOR_ID   0x10c4 /* Encontre o VendorID  do smartlamp */
#define PRODUCT_ID   0xea60  /* Encontre o ProductID do smartlamp */
static const struct usb_device_id id_table[] = { { USB_DEVICE(VENDOR_ID, PRODUCT_ID) }, {} };

static int  usb_probe(struct usb_interface *ifce, const struct usb_device_id *id); // Executado quando o dispositivo é conectado na USB
static void usb_disconnect(struct usb_interface *ifce);                           // Executado quando o dispositivo USB é desconectado da USB
static int usb_send_cmd(char *cmd, int param);
static int usb_read_serial(int mode);

static struct led_classdev smartlamp_led_cdev;

static void smartlamp_set_led_brightness(struct led_classdev *cdev, enum led_brightness brightness)
{
    char command_buffer[20];
    if (brightness != LED_OFF) {
        sprintf(command_buffer, "SET_LED 100");
    } else {
        sprintf(command_buffer, "SET_LED 0");
    }
    usb_send_cmd(command_buffer, 3);
}

// Executado quando o arquivo /sys/kernel/smartlamp_sensors/* é lido (e.g., cat /sys/kernel/smartlamp_sensors/ldr)
static ssize_t attr_show(struct kobject *sys_obj, struct kobj_attribute *attr, char *buff);

// Variáveis para criar os arquivos no /sys/kernel/smartlamp_sensors/{ldr, temp, hum}
static struct kobj_attribute  ldr_attribute  = __ATTR(ldr, S_IRUGO, attr_show, NULL);
static struct kobj_attribute  temp_attribute = __ATTR(temp, S_IRUGO, attr_show, NULL);
static struct kobj_attribute  hum_attribute  = __ATTR(hum, S_IRUGO, attr_show, NULL);
static struct attribute      *attrs[]        = { &ldr_attribute.attr, &temp_attribute.attr, &hum_attribute.attr, NULL };
static struct attribute_group attr_group     = { .attrs = attrs };
static struct kobject        *sys_obj;       // Executado para ler a saida da porta serial

MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver smartlamp_driver = {
    .name        = "smartlamp",     // Nome do driver
    .probe       = usb_probe,       // Executado quando o dispositivo é conectado na USB
    .disconnect  = usb_disconnect,  // Executado quando o dispositivo é desconectado na USB
    .id_table    = id_table,        // Tabela com o VendorID e ProductID do dispositivo
};
module_usb_driver(smartlamp_driver);

// Executado quando o dispositivo é conectado na USB
static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct usb_endpoint_descriptor *usb_endpoint_in, *usb_endpoint_out;
    int error;

    smartlamp_device = interface_to_usbdev(interface);
    
    if(usb_find_common_endpoints(interface->cur_altsetting, NULL, &usb_endpoint_out, &usb_endpoint_in, NULL))
        return -EIO;

    // Detecta portas e aloca buffers de entrada e saída de dados na USB
    usb_max_size = usb_endpoint_maxp(usb_endpoint_in);
    usb_in = usb_endpoint_in->bEndpointAddress;
    usb_out = usb_endpoint_out->bEndpointAddress;
    usb_in_buffer = kmalloc(usb_max_size, GFP_KERNEL);
    usb_out_buffer = kmalloc(usb_max_size, GFP_KERNEL);

    // Cria arquivos do /sys/kernel/smartlamp_sensors/*
    sys_obj = kobject_create_and_add("smartlamp_sensors", kernel_kobj);
    if (!sys_obj)
        return -ENOMEM;
    
    error = sysfs_create_group(sys_obj, &attr_group);
    if (error) {
        kobject_put(sys_obj);
        return error;
    }

    smartlamp_led_cdev.name = "smartlamp::led";
    smartlamp_led_cdev.brightness_set = smartlamp_set_led_brightness;

    error = devm_led_classdev_register(&interface->dev, &smartlamp_led_cdev);
    if (error) {
        kobject_put(sys_obj);
        return error;
    }
    
    return 0;
}

// Executado quando o dispositivo USB é desconectado da USB
static void usb_disconnect(struct usb_interface *interface) {
    // Remove os arquivos em /sys/kernel/smartlamp
    kobject_put(sys_obj);
    // Desaloca buffers
    kfree(usb_in_buffer);
    kfree(usb_out_buffer);
}

// Envia um comando via USB, espera e retorna a resposta do dispositivo (convertido para int)
static int usb_send_cmd(char *cmd, int param) {
    int ret, actual_size;

    // preencha o buffer
    strcpy(usb_out_buffer, cmd);
    
    // Envia o comando (usb_out_buffer) para a USB
    // Procure a documentação da função usb_bulk_msg para entender os parâmetros
    ret = usb_bulk_msg(smartlamp_device, usb_sndbulkpipe(smartlamp_device, usb_out), usb_out_buffer, strlen(usb_out_buffer), &actual_size, 1000);
    if (ret) {
        printk(KERN_ERR "SmartLamp: Erro de codigo %d ao enviar comando!\n", ret);
        return ret;
    }

    return usb_read_serial(param);
}

static int usb_read_serial(int mode) {
    int ret, actual_size;
    int retries = 10; // Tenta algumas vezes receber uma resposta da USB. Depois desiste.
    long value;
    char prefix[20];
    char buffer[MAX_RECV_LINE];
    int buffer_idx = 0;

    if (mode == 1)
        strcpy(prefix, "RES GET_LDR ");
    else if (mode == 2)
        strcpy(prefix, "RES GET_LED ");
    else if (mode == 3)
        strcpy(prefix, "RES SET_LED ");
    else if (mode == 4)
        strcpy(prefix, "RES GET_TEMP ");
    else if (mode == 5)
        strcpy(prefix, "RES GET_HUM ");
    else
        return -EINVAL;

    const size_t prefix_len = strlen(prefix);

    // Espera pela resposta correta do dispositivo (desiste depois de várias tentativas)
    while (retries > 0) {
        // Lê os dados da porta serial e armazena em usb_in_buffer
        ret = usb_bulk_msg(smartlamp_device, usb_rcvbulkpipe(smartlamp_device, usb_in), usb_in_buffer, usb_max_size, &actual_size, 1000);
        if (ret) {
            retries--;
            continue;
        }

        if (actual_size > 0 && buffer_idx < (MAX_RECV_LINE - 1)) {
            memcpy(&buffer[buffer_idx], usb_in_buffer, actual_size);
            buffer_idx += actual_size;
            buffer[buffer_idx] = '\0';

            if (strchr(buffer, '\n')) {
                //caso tenha recebido a mensagem 'RES_LDR X' via serial acesse o buffer 'usb_in_buffer' e retorne apenas o valor da resposta X
                if (strncmp(buffer, prefix, prefix_len) == 0) {
                    //retorne o valor de X em inteiro
                    if (kstrtol(buffer + prefix_len, 10, &value) == 0) {
                        return (int)value; // A conversão funcionou.
                    }
                }
                buffer_idx = 0;
            }
        }
        retries--;
    }
    return -EIO;
}

// Executado quando o arquivo /sys/kernel/smartlamp_sensors/* é lido (e.g., cat /sys/kernel/smartlamp_sensors/ldr)
static ssize_t attr_show(struct kobject *sys_obj, struct kobj_attribute *attr, char *buff) {
    int value; // value representa o valor do sensor
    const char *attr_name = attr->attr.name; // attr_name representa o nome do arquivo que está sendo lido

    // Implemente a leitura do valor do ldr, temp ou hum usando a função usb_send_cmd()
    if (strcmp(attr_name, "ldr") == 0)
        value = usb_send_cmd("GET_LDR", 1);
    else if (strcmp(attr_name, "temp") == 0)
        value = usb_send_cmd("GET_TEMP", 4);
    else if (strcmp(attr_name, "hum") == 0)
        value = usb_send_cmd("GET_HUM", 5);
    else
        return -EINVAL;

    if (value < 0)
        return value;

    // Cria a mensagem com o valor do sensor
    return sprintf(buff, "%d\n", value);
}