# Controle Web de LED com ESP32

Controla um LED por PWM usando uma interface web local ou um potenciometro. O
ESP32 gerencia a propria conexao Wi-Fi, salva as preferencias e disponibiliza
uma pagina responsiva para celulares e computadores. O firmware tambem consulta
e instala releases versionados publicados no GitHub.

## Recursos

- Liga e desliga o LED pela interface web.
- Liga e desliga fisicamente por um eletrodo capacitivo no GPIO33.
- Usa uma interface dark responsiva por padrao.
- Ajusta a intensidade manualmente entre 0 e 100%.
- Alterna automaticamente para o ultimo controle movimentado.
- Configura o Wi-Fi por um portal cativo, sem senha da rede no codigo.
- Reconecta automaticamente a uma rede ja configurada.
- Pode ser acessado por `http://esp32-led.local` ou pelo endereco IP.
- Restaura modo, estado e intensidade manual depois de reiniciar.
- Mantem IP, leitura ADC e sinal Wi-Fi em uma secao recolhivel.
- Consulta atualizacoes no GitHub ao conectar e a cada 24 horas.
- Valida downloads OTA com HTTPS, target, tamanho e SHA-256.
- So instala uma nova versao depois da confirmacao do usuario.

Nao ha autenticacao na interface de controle. Qualquer dispositivo conectado a
mesma rede local pode controlar o LED ou iniciar uma atualizacao oficial.

## Hardware

- MH-ET LIVE ESP32 MiniKit
- Potenciometro linear de 10 kohms
- LED verde
- Resistor de 220 a 330 ohms
- Eletrodo capacitivo, como uma pequena placa de cobre ou aluminio
- Resistor de 1 kohm opcional para o eletrodo
- Protoboard e jumpers

A placa possui um LED azul controlavel no GPIO2 e um LED vermelho que normalmente
indica apenas a presenca de alimentacao. O firmware usa o azul para comunicar o
estado do dispositivo sem interferir no LED verde controlado pelo GPIO23.

## Ligacoes

Monte o circuito com a placa desligada.

### Potenciometro

```text
ESP32 3V3  -------- terminal externo
ESP32 GPIO34 ----- terminal central (cursor)
ESP32 GND  -------- outro terminal externo
```

Troque os dois terminais externos se o sentido de giro ficar invertido. O
GPIO34 pertence ao ADC1, nao possui funcao touch e pode ser usado
simultaneamente com o Wi-Fi.

### LED

```text
ESP32 GPIO23 ----- resistor 220/330 ohms ----- anodo do LED
                                                    |
                                               catodo ----- GND
```

O anodo geralmente e a perna mais longa. O lado achatado do encapsulamento
normalmente identifica o catodo. Nunca ligue o LED sem o resistor.

### Chave capacitiva

```text
ESP32 GPIO33 (T8) ----- resistor 1 kohm opcional ----- eletrodo metalico
```

O eletrodo nao deve ser ligado ao 3V3 nem ao GND. Use um fio curto e mantenha-o
afastado do LED controlado, do PWM e de fontes de ruido. O resistor em serie,
instalado proximo ao ESP32, e opcional e ajuda a limitar descargas acidentais.
Nao toque no eletrodo enquanto o LED azul estiver continuamente aceso durante a
inicializacao. O firmware mede o valor de referencia depois de iniciar o Wi-Fi,
nas mesmas condicoes eletricas usadas durante a operacao normal.

## Compilar e gravar

Abra o projeto no VS Code:

```bash
code /home/bruno/Projects/esp32-potenciometro-led
```

Use os comandos `PlatformIO: Build`, `PlatformIO: Upload` e
`PlatformIO: Serial Monitor`, ou execute no terminal:

```bash
pio run
pio run --target upload
pio device monitor
```

O monitor serial usa 115200 baud.

A versao compilada vem do arquivo `VERSION`. O build tambem inclui os sete
primeiros caracteres do commit Git atual. A tabela `partitions.csv` reserva dois
slots de 1.280 KiB para permitir atualizacao e rollback OTA.

## Preview local da interface

A interface pode ser validada sem ESP32 e sem instalar dependencias adicionais:

```bash
python3 scripts/web_preview.py
```

Abra `http://localhost:8000`. O servidor extrai diretamente o HTML de
`src/web/WebPage.h`, portanto o preview usa a mesma pagina incorporada ao
firmware. A API simulada permite testar o botao de energia, slider, restauracao,
reconfiguracao de Wi-Fi e o fluxo completo de OTA com progresso e reinicio.

Por padrao, a consulta OTA simulada oferece a proxima versao patch. Para escolher
outra versao:

```bash
python3 scripts/web_preview.py --update-version 2.0.0
```

Use `Ctrl+C` para encerrar. O preview valida HTML, CSS, JavaScript e integracao
com a API, mas nao substitui testes do PWM, potenciometro, Wi-Fi, TLS ou gravacao
nas particoes da placa.

## Primeira configuracao Wi-Fi

1. Grave o firmware e reinicie o ESP32.
2. No celular ou computador, procure uma rede com o nome
   `CONFIGURE-LED-XXXX`.
3. Conecte usando a senha `configure-led`.
4. O celular deve abrir o portal automaticamente ou exibir uma notificacao
   como `Entrar na rede` ou `Fazer login na rede`.
5. Escolha sua rede Wi-Fi e informe a senha dela.
6. Aguarde o ESP32 conectar e encerrar a rede temporaria.

Se o portal nao aparecer, desative temporariamente VPN, DNS privado ou dados
moveis e acesse `http://8.8.8.8` no navegador. Esse endereco e usado somente
na rede temporaria do ESP32 para melhorar a deteccao do portal em celulares
Android, especialmente Samsung.

As credenciais sao armazenadas na memoria interna do ESP32 pelo WiFiManager e
nao fazem parte do codigo-fonte ou do firmware compilado.

## Acessar o controle

Com o celular ou computador conectado a mesma rede do ESP32, abra:

```text
http://esp32-led.local
```

Se o dispositivo nao resolver nomes `.local`, use o IP mostrado no monitor
serial, por exemplo `http://192.168.1.120`.

O botao principal liga ou desliga a saida. O slider e o potenciometro ficam
sempre disponiveis: o ultimo que for movimentado assume automaticamente o
controle e liga o LED. O movimento fisico precisa variar pelo menos 2% em duas
leituras consecutivas, evitando que ruido eletrico roube o controle da web.

Um toque no eletrodo do GPIO33 alterna entre ligado e desligado sem mudar a
fonte nem o brilho selecionado. Manter o dedo sobre o eletrodo gera somente uma
alternancia; e preciso soltar antes do proximo toque. O estado e salvo junto com
as demais preferencias. Depois de desligar pelo toque, movimentar o
potenciometro continua assumindo o controle e religando o LED.

Quando a web assume, a posicao atual do potenciometro vira a nova referencia.
Ele so volta a controlar depois de ser realmente movimentado. A pagina mostra
sempre qual foi a ultima fonte utilizada.

## Indicador de status

O LED azul embarcado representa os estados mais importantes. Todos os padroes
sao temporizados sem bloquear o controle do LED, a interface web ou o OTA.

| Estado | Padrao do LED azul |
| --- | --- |
| Inicializando ou reiniciando | Aceso continuamente |
| Operacao normal | Pulso de 60 ms a cada 3 segundos |
| Portal de configuracao | Dois flashes curtos a cada 2 segundos |
| Reconectando ao Wi-Fi | 200 ms aceso e 800 ms apagado |
| Consultando atualizacao | 500 ms aceso e 500 ms apagado |
| Atualizacao disponivel | Dois flashes curtos a cada 5 segundos |
| Baixando firmware | Pisca rapidamente, 100 ms aceso e 100 ms apagado |
| Verificando firmware | Aceso com uma pausa de 100 ms a cada segundo |
| Erro OTA | Tres flashes curtos a cada 4 segundos |

Cada mudanca de estado tambem e registrada no monitor serial. O GPIO2 e um pino
de inicializacao do ESP32 e nao deve receber pull-up, resistor ou carga externa;
o uso pelo firmware se limita ao LED que ja faz parte da placa.

## Atualizacao do firmware

A secao `Informacoes do dispositivo` mostra a versao instalada e o estado da
consulta OTA. O ESP32 consulta automaticamente o ultimo release estavel depois
de conectar ao Wi-Fi e repete a consulta a cada 24 horas. Tambem e possivel usar
o botao `Verificar agora`.

Quando uma versao maior estiver disponivel, a pagina exibira `Atualizar agora`.
A instalacao exige confirmacao, baixa o binario para o slot inativo, confere o
SHA-256 publicado no manifesto e reinicia. Credenciais Wi-Fi e preferencias do
LED sao preservadas.

O dispositivo nao instala prereleases, versoes iguais, downgrades, binarios de
outra placa ou arquivos com tamanho/hash divergente. Se a imagem nova nao
concluir a inicializacao saudavel, o bootloader volta para a imagem anterior.

O acesso ao GitHub usa HTTPS com validacao de certificado e nao requer token,
pois o repositorio e publico. O relogio e sincronizado por NTP antes da conexao
TLS. A operacao falha de forma segura quando nao ha internet ou sincronizacao de
horario.

### Publicar uma versao

1. Atualize `VERSION` usando o formato `MAJOR.MINOR.PATCH`.
2. Commit as alteracoes.
3. Crie uma tag com o mesmo valor prefixado por `v`.
4. Envie o commit e a tag para o GitHub.

Exemplo:

```bash
git tag v1.1.0
git push origin main v1.1.0
```

O workflow `.github/workflows/release.yml` compila o firmware, verifica o limite
do slot OTA, gera `manifest.json` com tamanho e SHA-256 e publica os arquivos no
GitHub Release. Tags que nao correspondem ao arquivo `VERSION` sao rejeitadas.

Para mudar de rede, abra `Informacoes do dispositivo`, pressione `Configurar
outra rede Wi-Fi` e confirme. O ESP32 apaga somente as credenciais de rede,
reinicia e abre novamente o portal cativo. As preferencias do LED continuam
salvas.

## Restauracao de fabrica

Abra `Informacoes do dispositivo` e pressione `Restaurar configuracoes de
fabrica`. A pagina exibira uma confirmacao detalhada antes de executar a
operacao.

A restauracao apaga:

- Rede e senha Wi-Fi armazenadas.
- Estado ligado ou desligado.
- Modo de controle selecionado.
- Intensidade manual salva.

O firmware permanece instalado. Depois do reinicio, o LED fica desligado, a
fonte inicial volta para o potenciometro e a intensidade manual volta para 50%.
Conecte-se a rede `CONFIGURE-LED-XXXX` para configurar novamente o Wi-Fi.

## API local

A interface usa uma API HTTP simples, que tambem pode ser acessada por outras
aplicacoes na rede local.

### Consultar estado

```http
GET /api/state
```

### Alterar controles

```http
POST /api/control
Content-Type: application/x-www-form-urlencoded

brightness=75
```

Pelo menos um campo deve ser enviado. `power` aceita `true`, `false`, `1` ou
`0`; `brightness` aceita inteiros de 0 a 100. Enviar `brightness` assume o
controle pela web e liga o LED automaticamente. O campo `mode` da resposta e
somente informativo e indica a ultima fonte utilizada.

### Reconfigurar Wi-Fi

```http
POST /api/wifi/reset
```

### Restaurar configuracoes de fabrica

```http
POST /api/factory-reset
```

Esse endpoint e destrutivo: apaga as preferencias do LED e as credenciais
Wi-Fi antes de reiniciar o dispositivo.

### Consultar atualizacoes

```http
POST /api/ota/check
```

Agenda uma consulta ao ultimo release do GitHub e responde com HTTP `202`.

### Instalar atualizacao

```http
POST /api/ota/install
```

Revalida o manifesto e instala a versao disponivel. Retorna HTTP `409` quando
nao existe uma versao maior ou outra operacao OTA esta em andamento.

`GET /api/state` tambem informa `firmwareVersion`, `firmwareBuild`, `otaStatus`,
`otaAvailableVersion`, `otaProgress`, `otaLastCheck`, `otaUpdateAvailable` e
`otaError`. Requisicoes `POST` devem reenviar no cabecalho `X-CSRF-Token` o
valor `csrfToken` obtido nessa resposta.

## Organizacao do projeto

```text
include/
  AppConfig.h                 pinos, PWM, rede e intervalos
  AppState.h                  estado compartilhado da aplicacao
  GitHubCertificates.h        autoridades certificadoras usadas pelo OTA
scripts/
  generate_manifest.py        geracao e validacao do manifesto de release
  version.py                  injecao da versao e commit no build
src/
  controllers/
    LedController.*           potenciometro, touch e aplicacao do PWM
  input/
    TouchButtonState.*        debounce e rearme testados no build
  services/
    NetworkManager.*          Wi-Fi, portal cativo e mDNS
    OtaUpdateService.*        consulta, download, validacao e rollback OTA
    SettingsStore.*           preferencias persistentes
    StatusIndicator.*         prioridade e acionamento do LED azul
    WebServerService.*        rotas HTTP e validacao da API
  status/
    StatusPattern.*           padroes temporais testados no build
  web/
    WebPage.h                 HTML, CSS e JavaScript embarcados
  main.cpp                    inicializacao e coordenacao dos modulos
```

O servidor HTTP, TLS, Update e o armazenamento `Preferences` fazem parte do
framework Arduino para ESP32. As bibliotecas externas sao `WiFiManager` e
`ArduinoJson`, declaradas no `platformio.ini`.

## Detalhes tecnicos

- ADC e PWM usam 12 bits, com valores entre 0 e 4095.
- O PWM opera em 5 kHz no canal 0.
- O indicador de status usa saida digital no GPIO2 e nao ocupa canal PWM.
- A chave capacitiva usa o canal touch T8 no GPIO33, com leitura filtrada pelo
  driver do ESP-IDF e avaliacao a cada 20 ms.
- O touch descarta as quatro amostras mais altas e mais baixas da calibracao,
  rejeita calibracoes instaveis e usa histerese. A pressao precisa permanecer
  valida por duas leituras; uma amostra isolada ou invalida nao aciona o LED.
- O rearme exige uma soltura estavel por 300 ms. Se a entrada permanecer ativa
  por 15 segundos, o firmware aguarda uma soltura confiavel antes de recalibrar.
- O potenciometro usa GPIO34, que nao e canal touch. O PWM usa GPIO23, fora do
  dominio RTC e afastado fisicamente do GPIO33 no chip.
- A CPU opera em 160 MHz e o loop libera o processador por 4 ms entre ciclos.
- O Wi-Fi usa `WIFI_PS_MIN_MODEM`, preservando baixa latencia, mDNS e multicast.
- Com o potenciometro ativo, cada leitura usa a media de 8 amostras a cada
  20 ms. Sob controle web ou com o LED desligado, usa 4 amostras a cada 40 ms.
- O potenciometro assume apos variar 2% em duas leituras consecutivas.
- A interface consulta o estado a cada segundo enquanto visivel, a cada 15
  segundos em segundo plano e a cada 500 ms durante uma operacao OTA.
- Alteracoes do slider sao limitadas para evitar excesso de requisicoes.
- Gravacoes de preferencias sao agrupadas e atrasadas em um segundo para
  reduzir o desgaste da memoria flash, inclusive quando o estado muda por toque.
- O download OTA ocorre em uma tarefa separada e usa blocos de 4 KiB.
- A imagem so e ativada depois que tamanho, target e SHA-256 forem validados.

`Light sleep` manual e `deep sleep` nao sao usados porque interromperiam o
servidor web, mDNS, Wi-Fi ou PWM. O modo `WIFI_PS_MAX_MODEM` tambem nao e
habilitado por padrao porque pode perder trafego multicast em alguns roteadores.

## Acesso a porta serial no Ubuntu

Se o upload falhar com `Permission denied`, adicione o usuario ao grupo da
porta serial:

```bash
sudo usermod -aG dialout "$USER"
```

Depois encerre completamente a sessao do Ubuntu e entre novamente. Confirme a
placa com `pio device list`.
