# chat2019-1

Este repositório compreende o trabalho 1 de Fundamentos de Sistemas operacionais. 

Nele, foram implementados os seguintes protocolos:

- Filas POSIX no padrão chat-usuario para cada usuário
- Envio de mensagens para a fila no padrão DE:PARA:MENSAGEM
- Apresentação da mensagem no formato De: MSG
- As filas criadas possuem permissão de leitura e escrita para o dono e apenas escrita para os demais. 
- Bloqueio de saída com ^C através do tratamento do sinal SIGINT
- Apresenta  UNKNOWNUSER PARA se fila do destinatário não existe
- Quando não é possível enviar mensagem para uma fila, tenta três vezes. Se ainda assim não conseguir imprime ERRO DE:PARA:MSG
- Comando list para listar usuários online
- BroadCast ao usar comando @all msg 
- Thread para esperar mensagem e para enviar 

Problemas conhecidos: 

- Não foi realizado teste de lotação da fila de mensagens ou de fluxos grandes de mensagens.

Dificuldades de implementação do modelo de threads:

- Perceber a necessidade de "esperar" o retorno de uma thread

## Como usar 

0. Compile com: gcc t1.c -lrt -lpthread e execute: ./a.out 
1. Ao entrar, digite um nome de usuário
2. Digite list para verificar os usuários online
3. Escolha um usuário e envie uma mensagem com o comando: @usuario Mensagem
4. Caso queira enviar uma mensagem para todos, digite : @all Mensagem
5. Para sair do chat, digite: sair