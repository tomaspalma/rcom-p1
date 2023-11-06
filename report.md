---
fontsize: 11pt
title: RCOM - 1º Projeto
author:
- Diogo Alexandre Soares Martins (202108883) 
- Tomás Figueiredo Marques Palma (202108880)
geometry: "left=2cm, right=2cm, top=1cm"
---

# 1. Introdução

O trabalho de redes de computadores teve como objetivo a construção de um protocolo de dados de baixo nível para a
transmissão de bytes, assim como a construção de uma aplicação para transferência de ficheiros que utiliza a camada
anteriormente referida, servindo este relatório como meio de documentação das camadas desenvolvidas, assim como um documento
com conclusões sobre a eficiência e sobre o quão corretos os móduluos desenvolvidos são.

No relatório, primeiramente ir-se-á encontrar documentado detalhes sobre
a arquiterura, a estrutura do código e os casos de uso
principais das camadas desenvolvidas e de seguida falar-se-à com detalhe acerca de cada um dos protocolos
desenvolvidos (o da ligação lógica e o da aplicação),
sendo, depois, por fim, abordados a correção e a eficiência do programa.

# 2. Arquitetura

Será descrito os diferentes módulos e como interagem entre si.

## 2.1. Módulo Porta Série (```serial_port.c```)

Este módulo é responsável pela configuração inicial da porta série
de modo a ficar separado das outras camadas
de modo a que saibam o mínimo de informações internas, como o ```baudrate```, aumentando a modularidade.

## 2.2. Módulo de ligação de dados (```link_layer.c```)

Este módulo está acima do módulo da porta série, sendo este o responsável por invocar a API da porta série,
de modo a começar a configuração inicial da mesma, assim como o responsável por ter o conhecimento acerca de como
transferir e receber uma trama quer seja ela de informação ou de supervisão, fazendo a verificação de erros
e obrigando a uma retransmissão em caso de falha.

## 2.3. Módulo da aplicação (```application_layer.c```)

Este módulo implementa a funcionalidade quer de transferência como de receção de ficheiros de entre duas portas série,
invocando a API exposta pelo protocolo de ligação de dados, sem nunca ter conhecimento interno de como transferir
uma trama de um lado para o outro, simplesmente apenas tendo o conhecimento de como pedir ao módulo de ligação de dados
para transferir pacotes, assim como codificar e descodificar
informação do ficheiro como o seu nome, tamanho e conteúdo de modo a que as duas partes estejam sincronizadas
num mesmo protocolo.

## 2.4. Módulo de *utils* (```utils.c```)

### 2.4.1. Relação com a arquitetura

Este módulo implementa uma API com funcionalidades fora da responsabilidade de outros módulos que em termos tal como
teóricos poderiam ser usados em qualquer módulo.

### 2.4.2. Principais funções (API)

```c
int get_size_of_file(FILE *file);
int get_no_of_bits(int n);
```

# 3. Estrutura do código

## 3.1. Módulo porta série

Este módulo é responsável pela configuração inicial da porta série.

```c
#define BAUDRATE 115200

int setup_port(int fd);
```

## 3.2. Módulo da ligação de dados

### 3.2.1. Relação com a arquitetura

Este módulo é o responsável pela transmissão e receção de bytes de uma porta série, sem ter o conhecimento do que é que os bytes,
para além dos de controlo especificados pelo guião do trabalho, significam.

### 3.2.2. Principais funções (API)

```c
int llopen(LinkLayer connectionParameters);
int llwrite(const unsigned char *buf, int bufSize);
int llread(unsigned char *packet);
int llclose(LinkLayerRole role, int showStatistics);
int show_statistics(LinkLayerRole role);
int llclose_transmitter();
int llclose_receiver();
```

### 3.2.3. Principais estruturas de dados

```c
typedef enum {
  LlTx,
  LlRx,
} LinkLayerRole;
```

- É utilizado como uma forma mais amigável ao programador de identificar se o programa está a ser executado como recetor
ou transmissor.

```c
typedef struct {
  char serialPort[50];
  LinkLayerRole role;
  int baudRate;
  int nRetransmissions;
  int timeout;
} LinkLayer;
```

- É utilizado como um aglomerado de informação acerca da configuração de como o protocolo de ligação de dados irá funcionar e com que
tipo de configuração da porta série.

## 3.3. Módulo da aplicação

### 3.3.1. Relação com a arquitetura

É o módulo responsável pela funcionalidade de transmitir e receber ficheiros.

### 3.3.2. Principais funções (API)

```c
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);
int transmitter_application_layer(const char *filename);
int receiver_application_layer();
unsigned char *read_control_frame(int app_layer_control, int *file_size);
int read_file(int file_size, unsigned char *filename, 
    FILE *file, int *current_size);
int send_file(FILE *file, int file_size);
int send_control_frame(const char *filename, int file_size, 
    int app_layer_control);
```

### 3.3.3. Principais estruturas de dados

Foram criadas as seguintes estruturas de dados auxiliares:

```c
typedef enum {
  READ_CONTROL_START,
  READ_FILESIZE,
  READ_FILENAME,
} READ_CONTROL_STATE;

typedef enum {
  READ_CONTROL_DATA,
  READ_DATA_L1,
  READ_DATA_L2,
  READ_DATA
} READ_FILE_STATE;
```

São estados utilizados na máquinas de estados de leitura do ficheiro.
```c
int get_size_of_file(FILE *file);
int get_no_of_bits(int n);
```
# 4. Casos de usos principais

Em cada um dos casos estará descrito o fluxo lógico de execução.

## 4.1. Receber um ficheiro

A lógica inicia-se na função ```receiver_application_layer``` do ficheiro ```application_layer.c```.

1. Receber a trama de conrole de início a partir da rotina ```read_control_frame(CONTROL_START, &file_size_start)``` que vai atribuir o tamanho do ficheiro e o nome do ficheiro a variáveis.

2. Abrir of ficheiro onde se vai escrever com ```fopen```.

3. Depois de recebida a trama de controlo inicial, vai-se recebendo tramas de informação com a função ```read_file```, até termos recebido o número de bytes igual ao indicado pela trama de controlo.

4. Por fim, recebemos a trama de controlo de fim a partir da rotina ```read_control_frame(CONTROL_END, &file_size_stop)```.

## 4.2. Enviar um ficheiro

A lógica inicia-se na função ```transmitter_application_layer``` do ficheiro ```application_layer.c```.

1. Ler o ficheiro para memória a partir com o ```fread```.

2. Para além do nome do ficheiro que recebe a partir da função ```main```, é necessário obter o tamanho do ficheiro
aberto pelo ```fread```, que se faz a partir da função ```get_size_of_file``` do ```utils.c```.

3. De seguida, é construída e enviada a trama de controlo de início cuja especificação vai de encontro ao guião do trabalho,
a partir da rotina ```send_control_frame(filename, file_size, CONTROL_START)```.

4. Depois de transferida a trama de controlo inicial, vai-se enviando ```MAX_DATAFIELD_SIZE``` bytes do ficheiro de cada vez, a partir da função ```send_file```
execeto quando se chega ao momento em que a quantidade de bytes que resta enviar seja inferior a ```MAX_DATAFIELD_SIZE```,
circunstâncias essas onde se envia ```Tamanho restante do ficheiro``` bytes.

5. Por fim, após a transferência do ficheiro, enviamos a trama de controlo de fim que repete a mesma informação
que a que a trama de controlo de início continha, a partir da rotina ```send_control_frame(filename, file_size, CONTROL_START)```.

# 5. Protocolo de ligação de dados

Esta camada é a camada implementada por nós que é a de mais baixo nível, sendo ela a utilizada pelo protocolo de aplicação
para transferir e receber ficheiros.

## 5.1. ```llopen()```

Uma das responsabilidades desta função é a preparação da porta série para transmissão e receção, configurando-a com os parâmetros pretendidos,
sendo de seguida iniciado o processo de estabelecimento de comunicação entre as duas partes como descrito no guião do trabalho, em que o transmissor envia uma trama de supervisão ```SET``` e fica à espera de receber um ```UA``` do recetor,
caso em que depois permite a execução da camada de aplicação.

## 5.2. ``llwrite()``

É responsável pela escrita de tramas de informação para a porta série recetora, processo no qual efetua um mecanismo de *byte stuffing*, caso
existam bytes nos ficheiros a transferir que tenham significado para o protocolo de ligação de dados tal como é o caso do ```0x7e``` que indica
o início e fim de uma trama, assim comocalcula o $bcc$ dos bytes enviados, não considerando
os bytes adicionais resultantes do *byte stuffing*, colocando-os na trama de informação e, de seguida, fica à espera da resposta do recetor,
retransmitindo a trama caso o recetor indique que detetou erro, ao calcular o bcc de novo.

## 5.3. ``lread()``

É responsável pela leitura de tramas de informação recebidas pelo transmissor, tendo de preoceder a um *destuffing* dos bytes, assim como
verificando se o bcc calculado ao ler os bytes sem contar com os bytes adicionais do *stuffing* é igual ao que foi enviado pelo transmissor
na trama de informação e, caso não seja, tem de enviar uma trama de supervisão de rejeição ```REJ(nr)``` para que o transmissor volte a reenviar. Caso cálculo do *bcc* bata certo, envia uma trama de supervisão de aceitação ```RR(nr)```.

## 5.4. ``llclose()``

É responsável pelo fecho da conexão, onde o transmissor envia um ```DISC``` para o recetor, ficando à espera de receber um ```DISC``` de volta
do recetor, de seguida enviando um ```UA```, fechando a conexão. Por sua vez, o recetor apenas envia um ```DISC``` após receber o primeiro ```DISC``` do transmissor e depois fecha a conexão com a porta série.

# 6. Protocolo de aplicação

Esta camada é a camada de mais alto nível implementada por nós, sendo utilizada para interagir com o utilizador e com o ficheiro a ser transferido, utilizando a API do protocolo de ligação de dados.

## 6.1. ```receiver_application_layer```

É responsável por receber, primeiramente, a trama de controlo de início, que contém informacação sobre o nome e tamanho do ficheiro, seguida das tramas de informação, com as quais preenche um novo ficheiro (o ficheiro transferido), e, por fim, a trama de controlo de fim, que repete os dados transferidos na trama de controlo de início.

## 6.2. ```transmitter_application_layer```

É responsável por enviar o ficheiro desejado para o outro computador, começando pela trama de controlo de início, seguida das tramas de informação, que são geradas dividindo o ficheiro de modo que sejam o menor número possível de tramas, em que todas exceto a última têm tamamho ```MAX_DATAFIELD_SIZE```, e, por fim, a trama de controlo de fim.

# 7. Validação

Este programa foi testado quer em ambiente de laboratório na FEUP quer virtualmente a partir do ```cable.c```
fornecido pela equipa docente.

Em caso de erro, o programa consegue recuperar, o que significa que o transmissor recebe a trama de rejeição
do recetor, não enviando mais nenhuma trama a seguir, tentando simplesmente reenviar a trama que chegou ao
recetor com erro, até o número de tentativas chegar a ```MAX_TRIES``` de ```TIMEOUT``` em ```TIMEOUT``` segundos.

Em caso de desconexão espontânea, o mecanismo de retransmissão comprovou-se fiável, sendo que após uma desconexão de mais
que ```TIMEOUT``` segundos, o programa consegue recuperar.

Os mecanismos de retransmissão foram também testados a correr o transmissor antes do recetor, comprovando que o transmissor,
após não receber uma resposta após 4 tentativas, aborta a conexão.

O programa também sucedeu com ficheiros de diferentes tamanhos:

- O ficheiro fornecido (penguin.gif) de 10968 bytes

- Um outro ficheiro introduzido por nós de 31802 bytes

# 8. Eficiência do protocolo de ligação de dados

## 8.1. Variar ```Frame Error Ratio``` 

## 8.2. Variar tempo de propagação

## 8.3. Variar capacidade de ligação

## 8.4. Variar tamanho das tramas

# 9. Conclusões

Em suma, o nosso programa tem duas camadas principais, isoladas entre si em que a de ligação
de dados expõe uma API para a camada da aplicação, de modo a que consigamos efetuar a transferência
de um ficheiro de uma porta série para outra.

Para além disso, em termos de aprendizagem, este trabalho foi bastante útil para uma melhor
compreensão acerca de *byte stuffing*, assim como o mecanismo de transmissão e controlo de erros ```Stop and Wait```, tal como uma primeira experiência real com um dispositivo físico onde nem tudo funciona
tão bem como num meio virtual.