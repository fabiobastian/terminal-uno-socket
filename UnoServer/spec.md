# UNO — Especificação do Client

## 1. Objetivo

O Client é uma aplicação de terminal responsável por:

* Conectar ao servidor UNO via TCP.
* Receber as mensagens enviadas pelo servidor.
* Exibir o estado atual da partida.
* Permitir que o jogador realize ações.
* Enviar solicitações de ações ao servidor.
* Atualizar a interface de acordo com as respostas recebidas.

O Client **não possui as regras do jogo**.

A validação das jogadas, alteração do estado da partida, distribuição das cartas e controle dos turnos são responsabilidades do Server.

---

# 2. Comunicação

A comunicação utiliza:

* TCP
* IPv4
* Winsock 2
* Porta `8080`

Por padrão, durante os testes:

```text
IP: 127.0.0.1
Porta: 8080
```

A comunicação utiliza estruturas C compartilhadas através de `protocol.h`.

O Client deve utilizar:

```c
send()
recv()
```

Porém, como TCP não garante que uma chamada de `send()` ou `recv()` processe todos os bytes solicitados, devem ser utilizadas funções equivalentes a:

```c
enviarTudo()
recvAll()
```

para transmitir estruturas completas.

---

# 3. Estruturas do protocolo

## 3.1 Carta

```c
typedef struct {
    int id;
    char simbolo[MAX_SIMBOLO_CARTA];
    Cor cor;
} Carta;
```

Cada carta possui:

* `id`: identificador único da carta.
* `simbolo`: número da carta (`0` a `9`).
* `cor`: cor da carta.

---

# 4. Jogador

```c
typedef struct {
    int id;
    char nome[MAX_NOME_JOGADOR];

    int qtdCartas;
    Carta cartas[MAX_QTD_CARTAS_JOGADOR];

    int qtdCartasPossiveis;
    int idsCartasPossiveis[MAX_QTD_CARTAS_JOGADOR];

} Jogador;
```

O Client recebe:

### Próprio jogador

O Client recebe:

* ID
* Nome
* Todas as suas cartas
* Quantidade de cartas
* IDs das cartas que podem ser jogadas

### Adversário

O Client **não recebe as cartas do adversário**.

Recebe somente:

* Nome
* Quantidade de cartas

---

# 5. Estado da partida

```c
typedef struct {
    int numeroRodada;
    bool suaVez;
    int tempoMs;

    Carta ultimaCarta;

    int numeroCartasAdversario;
    char nomeAdversario[MAX_NOME_JOGADOR];

} Partida;
```

O Client utiliza essas informações para apresentar:

* Número da rodada.
* Se é ou não a vez do jogador.
* Tempo disponível para jogar.
* Última carta jogada.
* Quantidade de cartas do adversário.
* Nome do adversário.

---

# 6. Estado completo

```c
typedef struct {
    Jogador jogador;
    Partida partida;
} EstadoJogo;
```

Cada jogador recebe uma visão própria da partida.

Por exemplo:

```text
Cliente 1:

jogador.id = 0
jogador.nome = Jogador 1
jogador.cartas = cartas do Jogador 1

partida.nomeAdversario = Jogador 2
partida.numeroCartasAdversario = 6
```

Já o Cliente 2 receberá:

```text
jogador.id = 1
jogador.nome = Jogador 2
jogador.cartas = cartas do Jogador 2

partida.nomeAdversario = Jogador 1
partida.numeroCartasAdversario = 6
```

---

# 7. Mensagens

O protocolo possui:

```c
typedef enum {
    MSG_PARTIDA_INICIADA,
    MSG_ESTADO_JOGO,
    MSG_JOGADA_INVALIDA,
    MSG_PARTIDA_FINALIZADA,
    MSG_JOGADOR_SAIU
} TipoMensagem;
```

A mensagem completa:

```c
typedef struct {
    TipoMensagem tipo;
    EstadoJogo estado;
} Mensagem;
```

---

# 8. Solicitações

As ações enviadas pelo Client utilizam:

```c
typedef enum {
    ACAO_JOGAR_CARTA,
    ACAO_COMPRAR_CARTA,
    ACAO_DIZER_UNO
} TipoAcao;
```

A solicitação:

```c
typedef struct {
    int jogadorId;
    TipoAcao acao;
    int cartaId;
} Solicitacao;
```

---

# 9. Fluxo de conexão

Ao iniciar:

```text
Client
  │
  │ WSAStartup
  │
  │ socket()
  │
  │ connect()
  ▼
Server
```

Após conectar, o Client **não deve enviar nenhuma ação imediatamente**.

Ele deve aguardar a primeira mensagem do Server.

---

# 10. Início da partida

A primeira mensagem enviada pelo Server para cada Client será obrigatoriamente:

```c
MSG_PARTIDA_INICIADA
```

O Client deve validar:

```c
if (response.tipo == MSG_PARTIDA_INICIADA)
```

Somente depois dessa mensagem a partida é considerada iniciada pelo Client.

Fluxo:

```text
Client 1 ────────┐
                 │
Client 2 ────────┤
                 ▼
              Server
                 │
          inicia partida
                 │
        ┌────────┴────────┐
        ▼                 ▼
    Client 1          Client 2
        │                 │
        │ MSG_PARTIDA     │ MSG_PARTIDA
        │ _INICIADA       │ _INICIADA
        ▼                 ▼
     estado             estado
```

---

# 11. Regras do jogo

## 11.1 Quantidade de jogadores

A partida possui exatamente:

```text
2 jogadores
```

---

## 11.2 Baralho

O baralho possui somente cartas numéricas.

São utilizadas quatro cores:

```text
AMARELO
VERMELHO
VERDE
AZUL
```

Cada cor possui:

```text
0 1 2 3 4 5 6 7 8 9
```

Total:

```text
4 × 10 = 40 cartas
```

Cada carta possui um ID único.

---

# 12. Cartas iniciais

Cada jogador começa com:

```text
7 cartas
```

Portanto:

```text
Jogador 1 → 7 cartas
Jogador 2 → 7 cartas
```

---

# 13. Carta inicial da partida

Após distribuir as cartas, o Server define uma carta inicial como:

```c
ultimaCarta
```

Essa carta será utilizada para determinar quais cartas podem ser jogadas.

---

# 14. Regra para jogar uma carta

Uma carta pode ser jogada quando possuir:

### Mesma cor

```text
carta.cor == ultimaCarta.cor
```

OU

### Mesmo número

```text
carta.simbolo == ultimaCarta.simbolo
```

Exemplo:

```text
Última carta:

VERDE 5
```

Podem ser jogadas:

```text
VERDE 2
VERDE 8
VERDE 5

AMARELO 5
VERMELHO 5
AZUL 5
```

---

# 15. Cartas possíveis

O Server calcula as cartas que podem ser jogadas.

O resultado é enviado através de:

```c
qtdCartasPossiveis
idsCartasPossiveis[]
```

Exemplo:

```text
Quantidade de cartas possíveis: 3

Cartas possíveis:

1 - Carta ID 12
2 - Carta ID 25
3 - Carta ID 31
```

O Client deve utilizar esses IDs para auxiliar o jogador na escolha.

O Client **não deve ser responsável por validar a jogada**.

Mesmo que uma carta não esteja em `idsCartasPossiveis`, o Client pode enviar a solicitação. O Server será responsável pela validação.

---

# 16. Turnos

Somente um jogador pode jogar por vez.

O estado contém:

```c
partida.suaVez
```

Se:

```c
suaVez == true
```

o Client deve permitir que o jogador escolha uma ação.

Se:

```c
suaVez == false
```

o Client deve aguardar uma mensagem do Server.

Exemplo:

```text
Cliente 1:

Sua vez: SIM

1 - Jogar carta
2 - Comprar carta
```

Enquanto:

```text
Cliente 2:

Sua vez: NAO

Aguardando jogada do adversário...
```

---

# 17. Jogar carta

Quando for sua vez, o Client poderá selecionar:

```text
1 - Jogar carta
```

O jogador informa o ID da carta.

O Client envia:

```c
Solicitacao request;

request.jogadorId = jogador.id;
request.acao = ACAO_JOGAR_CARTA;
request.cartaId = idDaCarta;
```

O Client envia a estrutura inteira através do TCP.

---

# 18. Compra de carta

Quando for sua vez, o jogador também poderá escolher:

```text
2 - Comprar carta
```

Nesse caso:

```c
request.jogadorId = jogador.id;
request.acao = ACAO_COMPRAR_CARTA;
request.cartaId = 0;
```

O `cartaId` não é utilizado nessa ação.

Após comprar uma carta:

```text
Jogador compra carta
        │
        ▼
Turno termina
        │
        ▼
Próximo jogador
```

---

# 19. Jogada válida

Quando uma jogada for válida, o Server altera o estado do jogo.

Depois disso, o novo estado deve ser enviado para **ambos os jogadores**.

```text
Jogador 1 joga
      │
      ▼
Server valida
      │
      ▼
Estado alterado
      │
      ├───────────────┐
      ▼               ▼
 Cliente 1        Cliente 2
```

Cada Client recebe seu próprio `EstadoJogo`.

O jogador que realizou a jogada poderá receber:

```c
MSG_ESTADO_JOGO
```

e o adversário também receberá:

```c
MSG_ESTADO_JOGO
```

---

# 20. Atualização do adversário

Esse comportamento é obrigatório para manter os Clients sincronizados.

Exemplo:

```text
Estado inicial:

Jogador 1 → suaVez = SIM
Jogador 2 → suaVez = NAO
```

Jogador 1 joga uma carta.

Novo estado:

```text
Jogador 1 → suaVez = NAO
Jogador 2 → suaVez = SIM
```

O Server envia:

```text
                    MSG_ESTADO_JOGO
                       /       \
                      /         \
                     ▼           ▼
                Cliente 1    Cliente 2

                suaVez=NAO   suaVez=SIM
```

Assim o Cliente 2 deixa de aguardar e passa a permitir uma jogada.

---

# 21. Jogada inválida

Se o jogador tentar realizar uma jogada inválida, o Server envia:

```c
MSG_JOGADA_INVALIDA
```

A princípio, essa mensagem é enviada somente para o jogador que realizou a tentativa.

O estado do jogo não é alterado.

O Client deve:

1. Informar que a jogada foi inválida.
2. Exibir novamente o estado atual.
3. Permitir que o jogador tente novamente.

Exemplo:

```text
>>> JOGADA INVALIDA <<<

Última carta: VERDE 5

Cartas possíveis:
- Carta 12
- Carta 25
- Carta 31

Escolha uma nova ação:
```

---

# 22. Vitória

Quando um jogador ficar com:

```text
0 cartas
```

a partida termina.

O Server deve enviar:

```c
MSG_PARTIDA_FINALIZADA
```

para os dois jogadores.

O Client deve:

1. Informar que a partida terminou.
2. Exibir o estado final.
3. Encerrar o loop da partida.
4. Desconectar do Server.

---

# 23. Jogador desconectado

Se um jogador sair da partida, o Server poderá enviar:

```c
MSG_JOGADOR_SAIU
```

O Client deve:

1. Informar que o adversário saiu.
2. Encerrar a partida localmente.
3. Fechar o socket.
4. Encerrar a aplicação.

---

# 24. Fluxo geral do Client

O fluxo esperado é:

```text
┌──────────────────────┐
│      Inicializar     │
│       Winsock        │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│     Criar Socket     │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│       Connect        │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ Aguardar mensagem    │
│ do servidor          │
└──────────┬───────────┘
           │
           ▼
     MSG_PARTIDA_INICIADA
           │
           ▼
┌──────────────────────┐
│  Exibir estado       │
│  inicial             │
└──────────┬───────────┘
           │
           ▼
      ┌────────────┐
      │ suaVez ?   │
      └─────┬──────┘
        SIM │ NÃO
            │
     ┌──────┘ └──────────────┐
     ▼                       ▼
┌───────────────┐      ┌──────────────┐
│ Exibir ações  │      │ Aguardar     │
│               │      │ mensagem     │
│ 1 Jogar       │      │ do Server    │
│ 2 Comprar     │      └──────┬───────┘
└───────┬───────┘             │
        │                     │
        ▼                     │
┌───────────────┐             │
│ Enviar        │             │
│ Solicitacao   │             │
└───────┬───────┘             │
        │                     │
        └──────────┬──────────┘
                   ▼
          Receber Mensagem
                   │
          ┌────────┼───────────────┐
          ▼        ▼               ▼
       ESTADO   INVALIDA       FINALIZADA
          │        │               │
          └────────┴───────┐       │
                           ▼       ▼
                    Atualizar   Encerrar
                      estado     partida
```

---

# 25. Responsabilidades do Client

O Client deve:

* Gerenciar conexão TCP.
* Enviar `Solicitacao`.
* Receber `Mensagem`.
* Validar a ordem básica das mensagens.
* Interpretar `TipoMensagem`.
* Exibir as cartas do jogador.
* Exibir a última carta.
* Exibir as cartas possíveis.
* Exibir o estado do turno.
* Exibir informações do adversário.
* Solicitar ações do usuário.
* Encerrar corretamente a conexão.

---

# 26. Responsabilidades que NÃO pertencem ao Client

O Client não deve:

* Validar se uma carta pode ser jogada.
* Alterar `ultimaCarta`.
* Alterar o turno.
* Distribuir cartas.
* Criar o baralho.
* Embaralhar o baralho.
* Alterar quantidade de cartas.
* Determinar o vencedor.
* Controlar o estado oficial da partida.

O Server é a autoridade sobre o estado do jogo.

---

# 27. Regra importante de sincronização

O Client deve considerar o Server como a **fonte oficial do estado da partida**.

Portanto:

```text
Client envia ação
       │
       ▼
Server processa
       │
       ▼
Server altera estado
       │
       ▼
Server envia novo estado
       │
       ▼
Client atualiza sua interface
```

O Client não deve tentar antecipar alterações no estado.

Por exemplo, ao jogar uma carta, o Client não deve remover a carta de sua própria mão antes de receber a resposta do Server.

Ele deve aguardar:

```c
MSG_ESTADO_JOGO
```

e então utilizar o novo `EstadoJogo`.

---

# 28. Ordem esperada das mensagens

Para cada Client, a primeira mensagem recebida deve ser:

```text
MSG_PARTIDA_INICIADA
```

Depois disso, podem ocorrer:

```text
MSG_ESTADO_JOGO
MSG_JOGADA_INVALIDA
MSG_PARTIDA_FINALIZADA
MSG_JOGADOR_SAIU
```

Exemplo de uma partida:

```text
Client 1:

MSG_PARTIDA_INICIADA
        ↓
MSG_ESTADO_JOGO
        ↓
envia ACAO_JOGAR_CARTA
        ↓
MSG_ESTADO_JOGO
        ↓
aguarda
        ↓
MSG_ESTADO_JOGO
        ↓
envia ACAO_COMPRAR_CARTA
        ↓
MSG_ESTADO_JOGO
        ↓
MSG_PARTIDA_FINALIZADA
```

---

# 29. Tratamento de desconexão

Se:

```c
recv()
```

retornar:

```c
0
```

significa que o Server encerrou a conexão.

O Client deve:

```text
Informar desconexão
       ↓
Fechar socket
       ↓
WSACleanup()
       ↓
Encerrar
```

Se `recv()` retornar `SOCKET_ERROR`, o Client deve informar o erro e encerrar a conexão.

---

# 30. Interface inicial esperada

A interface pode ser simples.

Exemplo:

```text
==================================================
              PARTIDA INICIADA
==================================================

JOGADOR
------------------------------------------
ID: 0
Nome: Jogador 1
Quantidade de cartas: 7

SUAS CARTAS
------------------------------------------
1 - ID: 25 | cor: 2 | simbolo: 5
2 - ID: 31 | cor: 3 | simbolo: 1
3 - ID: 28 | cor: 2 | simbolo: 8
...

CARTAS POSSIVEIS DE JOGAR
------------------------------------------
1 - Carta ID 25
2 - Carta ID 28
3 - Carta ID 29

PARTIDA
------------------------------------------
Rodada: 1
Sua vez: SIM

ULTIMA CARTA
------------------------------------------
ID: 23 | cor: 2 | simbolo: 3

ADVERSARIO
------------------------------------------
Nome: Jogador 2
Quantidade de cartas: 7

==================================================

Sua vez!

1 - Jogar carta
2 - Comprar carta

Escolha:
```

Quando não for sua vez:

```text
==================================================
              AGUARDANDO ADVERSARIO
==================================================

Sua vez: NAO

Adversario: Jogador 2
Cartas do adversario: 6

Aguardando jogada do adversario...
```

---

# 31. Princípio geral

A arquitetura do sistema deve seguir:

```text
                 SERVER
                   │
          ┌────────┴────────┐
          │                 │
       REGRAS             ESTADO
          │                 │
          └────────┬────────┘
                   │
                 TCP
                   │
          ┌────────┴────────┐
          │                 │
       CLIENT 1          CLIENT 2
          │                 │
       Interface         Interface
```

O Server mantém o estado oficial.

Os Clients possuem apenas uma **visão do estado** enviada pelo Server.

Dessa forma, os dois jogadores permanecem sincronizados durante toda a partida.

```
```
