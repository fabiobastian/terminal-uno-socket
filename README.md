# terminal-uno-socket

Jogo **UNO de terminal**, desenvolvido em C, utilizando **Sockets TCP e Threads** para comunicação entre dois jogadores.

Projeto desenvolvido para a disciplina de **Redes de Computadores 1**.

## Tecnologias

* C11
* TCP / Sockets
* Windows Winsock2
* Threads
* CMake
* MinGW
* CLion

## Funcionamento

O projeto utiliza uma arquitetura **cliente-servidor**.

```text
             SERVIDOR
                |
        +-------+-------+
        |               |
    Jogador 1       Jogador 2
        |               |
      Cliente         Cliente
```

O servidor é responsável por:

* Aceitar os jogadores;
* Receber as jogadas;
* Controlar as regras do UNO;
* Manter o estado da partida;
* Enviar o estado atualizado para os jogadores.

Os clientes são responsáveis por:

* Conectar ao servidor;
* Exibir o jogo no terminal;
* Receber o estado da partida;
* Enviar as ações do jogador.

## Como executar

### 1. Requisitos

Para executar o projeto no Windows, é necessário ter:

* Windows;
* MinGW/GCC;
* CMake;
* CLion ou outro ambiente de desenvolvimento;
* Dois terminais para executar os clientes.

### 2. Compilar

Abra o projeto no **CLion** e faça o build.

O servidor e o cliente devem ser compilados separadamente.

### 3. Executar o servidor

Abra um terminal e execute o servidor:

```text
UnoServer.exe
```

O servidor deverá iniciar na porta:

```text
8080
```

Exemplo:

```text
Servidor escutando na porta 8080...
```

### 4. Executar os clientes

Abra **dois prompts de comando (CMD)** separados.

No primeiro:

```text
UnoClient.exe
```

No segundo:

```text
UnoClient.exe
```

O primeiro cliente conectado será o **Jogador 1** e o segundo será o **Jogador 2**.

### 5. Tamanho do terminal

O jogo possui uma interface desenhada diretamente no terminal e precisa de uma área mínima de aproximadamente:

```text
Largura: 150 colunas
Altura: 40 linhas
```

Por isso, antes de iniciar o cliente, recomenda-se aumentar o tamanho da janela do terminal.

Se a tela estiver pequena, o cliente poderá informar que o terminal não possui tamanho suficiente.

#### Dica

No **CMD**, é possível utilizar:

```text
CTRL + roda do mouse
```

para ajustar o zoom/tamanho da fonte do console, dependendo da configuração do Windows.

Também é possível diminuir o tamanho da fonte pelas configurações do terminal.

### 6. Cuidados durante a execução

* Inicie o **servidor antes dos clientes**.
* É necessário conectar **dois clientes** para iniciar a partida.
* Cada cliente deve ser executado em uma janela/terminal separado.
* Os clientes devem conectar na porta `8080`.
* Não execute dois servidores simultaneamente na mesma máquina utilizando a porta `8080`.
* Mantenha o terminal com tamanho suficiente para visualizar o tabuleiro.
* Evite redimensionar o terminal durante a partida.
* O servidor é responsável por controlar o estado oficial do jogo.
* Para encerrar o servidor de forma controlada, utilize:

```text
q + ENTER
```

## Exemplo de execução

Uma forma simples de testar o projeto é utilizar **três janelas de terminal**:

```text
┌──────────────────────────────┐
│ Terminal 1                   │
│                              │
│ UnoServer.exe                │
│ Servidor escutando           │
│ porta 8080                   │
└──────────────────────────────┘

┌──────────────────────────────┐
│ Terminal 2                   │
│                              │
│ UnoClient.exe                │
│ Jogador 1                    │
└──────────────────────────────┘

┌──────────────────────────────┐
│ Terminal 3                   │
│                              │
│ UnoClient.exe                │
│ Jogador 2                    │
└──────────────────────────────┘
```

Após os dois clientes se conectarem, o servidor inicia a partida e cada jogador recebe seu próprio estado do jogo.


## Estrutura do servidor

```text
UnoServer/
├── include/
│   ├── acceptor.h
│   ├── game.h
│   ├── listener.h
│   ├── protocol.h
│   ├── queue.h
│   ├── server.h
│   ├── worker.h
│   └── writer.h
│
├── src/
│   ├── acceptor.c
│   ├── game.c
│   ├── listener.c
│   ├── main.c
│   ├── queue.c
│   ├── server.c
│   ├── worker.c
│   └── writer.c
│
└── test/
    └── client.c
```

### Threads do servidor

```text
Acceptor
   ↓
aceita jogadores

Listener 1 ──┐
             ├── Request Queue → Worker → Response Queue → Writer
Listener 2 ──┘
```

* **Acceptor:** aceita as conexões dos dois jogadores.
* **Listener:** recebe requisições dos clientes.
* **Worker:** processa as jogadas e aplica as regras do jogo.
* **Writer:** envia as respostas para os clientes.
* **Queue:** faz a comunicação segura entre as threads.

## Regras do jogo

A implementação possui uma versão simplificada do UNO:

* 2 jogadores;
* 7 cartas iniciais para cada jogador;
* Cartas numéricas de `0` a `9`;
* 4 cores;
* Não possui cartas especiais;
* Uma carta pode ser jogada se possuir:

    * a mesma cor da carta atual; ou
    * o mesmo símbolo;
* Ao comprar uma carta, a vez passa para o outro jogador;
* O jogador que ficar sem cartas vence.

## Comunicação

A comunicação entre cliente e servidor utiliza **TCP**.

### Cliente → Servidor

O cliente envia uma `Solicitacao` contendo:

```text
jogadorId
acao
cartaId
```

As ações disponíveis são:

```text
ACAO_JOGAR_CARTA
ACAO_COMPRAR_CARTA
ACAO_DIZER_UNO
```

### Servidor → Cliente

O servidor envia uma `Mensagem` contendo o estado atualizado do jogo.

Tipos principais:

```text
MSG_PARTIDA_INICIADA
MSG_ESTADO_JOGO
MSG_JOGADA_INVALIDA
MSG_PARTIDA_FINALIZADA
MSG_JOGADOR_SAIU
```

## Porta

O servidor utiliza a porta:

```text
8080
```

Por padrão, o cliente conecta em:

```text
127.0.0.1:8080
```

## Como executar

### 1. Compilar o servidor

Abra o projeto no **CLion** e faça o build do `UnoServer`.

Ou compile utilizando o CMake configurado no projeto.

### 2. Iniciar o servidor

Execute:

```text
UnoServer.exe
```

O servidor deverá apresentar:

```text
Servidor escutando na porta 8080...
```

### 3. Iniciar os clientes

Abra **dois terminais** e execute um cliente em cada um.

```text
UnoClient.exe
```

O primeiro cliente será o jogador 1 e o segundo será o jogador 2.

### 4. Jogar

Controles:

```text
← →   Selecionar carta
ENTER Jogar carta
↓     Comprar carta
↑     UNO
ESC   Sair
```

## Encerramento

No servidor:

```text
q + ENTER
```

solicita o encerramento do servidor.

## Objetivo do projeto

O principal objetivo é demonstrar na prática conceitos de **Redes de Computadores**, principalmente:

* Comunicação TCP;
* Sockets;
* Cliente-servidor;
* Threads;
* Concorrência;
* Exclusão mútua;
* Filas produtor-consumidor;
* Comunicação entre threads;
* Estruturação de protocolos de comunicação.
