---
fontsize: 11pt
title: RCOM - 1º Projeto
author:
- Diogo Alexandre Soares Martins (202108883) 
- Tomás Figueiredo Marques Palma (202108880)
---

# 1. Introdução

O trabalho de redes de computadores teve como objetivo a construção de um protocolo de dados de baixo nível para a
transmissão de bytes, assim como a construção de uma aplicação para transferência de ficheiros que utiliza a camada
anteriormente referida, servindo este relatório como meio de documentação das camadas desenvolvidas, assim como um documento
com conclusões sobre a eficiência e sobre o quão corretos os móduluos desenvolvidos são.

No relatório, primeiramente ir-se-á encontrar documentado detalhes sobre
a arquiterura, a estrutura do código e os casos de uso
principais das camadas desenvolvidas.

De seguida, falar-se-à com detalhe acerca de cada um dos protocolos
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

Este módulo implementa uma API com funcionalidades fora da responsabilidade de outros módulos que em termos tal como
teóricos poderiam ser usados em qualquer módulo, como
a função para obter o tamanho de um ficheiro (```get_size_of_file```)
e a de obter o número de bits de um determinado número de base 10 (```get_no_of_bits```).

# 3. Estrutura do código

# 4. Casos de usos principais

Em cada um dos casos estará descrito o fluxo lógico de execução.

## 4.1. Receber um ficheiro

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

# 6. Protocolo de aplicação

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

# 9. Conclusões
