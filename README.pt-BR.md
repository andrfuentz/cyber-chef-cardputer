# MiseDeck

**Um mise en place de bolso e caixa de ferramentas culinária para o M5Stack Cardputer.**

O MiseDeck transforma o M5Stack Cardputer em um mise en place de bolso para escalar receitas, gerenciar ingredientes, usar timer, compartilhar receitas offline e acessar um portal local pelo navegador.

> *"nós somos os músicos e somos os sonhadores de sonhos"*

O MiseDeck foi idealizado por **André Fuentes** / **@anfuentz** e vibecodeado com **Codex**.

## Status

Release pública atual: **v1.0.0 EN**

Uma build em português brasileiro também está disponível como **v1.1 PT-BR**.

## O que ele faz

- Salva receitas no microSD
- Organiza receitas por categoria e favoritas
- Suporta receitas simples e compostas
- Recalcula receitas proporcionalmente pelo peso total
- Permite criar, duplicar, editar e apagar receitas no Cardputer
- Permite mover o cursor durante edição de textos e números
- Inclui modo rápido para cálculos proporcionais
- Inclui timer, conversor, tela de bateria e controle de som/volume
- Conecta ao Wi-Fi pelo próprio Cardputer
- Cria um portal local em `misedeck.local` ou pelo IP do aparelho
- Tem interface responsiva para celular
- Permite editar, salvar e baixar arquivos TXT de receita pelo navegador
- Compartilha receitas offline por QR Code

## Hardware

Pensado para:

- M5Stack Cardputer
- M5Stack Cardputer ADV / Cardputer-Adv

O projeto usa o target ESP32-S3 / StampS3 usado pelo Cardputer.

## Controles

Controles principais:

- `;` — cima
- `.` — baixo
- `,` — esquerda / anterior
- `/` — direita / próximo
- `OK` / `Enter` — confirmar
- `` ` `` / `Esc` — voltar / cancelar
- `Del` — apagar caractere
- `Tab` — favoritar/desfavoritar receita

Durante edição de texto ou número:

- `,` — mover cursor para esquerda
- `/` — mover cursor para direita
- `Del` — apagar antes do cursor

## Portal local

Depois de conectar o Cardputer ao Wi-Fi:

1. Abra `http://misedeck.local` no navegador, ou use o IP mostrado no Cardputer.
2. Navegue pelas receitas por categoria ou pela aba Todas.
3. Abra uma receita para visualizar, recalcular proporção, editar TXT, salvar ou baixar.

O portal roda direto no Cardputer dentro da rede local. Ele não é um serviço de nuvem e não precisa de conta.

## Formato TXT das receitas

Formato simples e legível:

```txt
FOCACCIA DO ANDRE
CATEGORIA: MASSAS
FAVORITA: SIM
TOTAL: 700.3

[INGREDIENTES]

AGUA|293.0|g
ACUCAR|8.0|g
AZEITE|20.0|g
FARINHA DE TRIGO|366.0|g
FERMENTO|3.3|g
SAL|10.0|g
```

Receitas compostas usam blocos `[PREPARO]`.

## Compartilhamento offline

O MiseDeck gera um QR Code com um resumo compacto da receita:

```text
Receita > Ações > Compartilhar
```

O QR não precisa de Wi-Fi. Ele contém nome da receita, peso total e ingredientes em texto simples.

## Compilação

Recomendado: PlatformIO.

```bash
pio run
pio run -t upload
```

Validado com:

- PlatformIO
- M5Cardputer `1.1.1`
- M5Unified `0.2.17`
- M5GFX `0.2.24`
- Arduino framework para ESP32

Veja [docs/INSTALL.md](docs/INSTALL.md) para instruções de gravação.

## Binários da release

- Inglês: `releases/v1.0.0/MiseDeck_Cardputer_v1.0.0_EN.bin`
- Português/Brasil: `releases/v1.1.0-PTBR/MiseDeck_Cardputer_v1.1_PT-BR.bin`

## Documentação

- [Instalação](docs/INSTALL.md)
- [Funcionalidades](docs/FEATURES.md)
- [Portal](docs/PORTAL.md)
- [Roadmap](docs/ROADMAP.md)
- [Checklist de release](docs/RELEASE_CHECKLIST.md)
- [README em inglês](README.md)

## Licença

MiseDeck é distribuído sob a [licença MIT](LICENSE).
