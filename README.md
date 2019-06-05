# chat2019-1

Este repositório compreende o trabalho 1 de Fundamentos de Sistemas operacionais. 

Nele, foram implementados os seguintes protocolos:

- Filas POSIX no padrão chat-usuario para cada usuário
- Envio de mensagens para a fila no padrão DE:PARA:MENSAGEM
- Apresentação da mensagem no formato De: MSG
- As filas criadas possuem permissão de leitura e escrita para o dono e apenas escrita para os demais. 
- Bloqueio de saída com ^C através do tratamento do sinal SIGINT
- Apresenta  UNKNOWNUSER PARA se fila do destinatário não existe
- Quando não é possível enviar mensagem para uma fila, tenta três vezes. Se ainda assim no conseguir imprime ERRO DE:PARA:MSG
- Comando list para listar usuários online
- BroadCast ao usar comando @all msg 
- Thread para esperar mensagem e para enviar 

Problemas conhecidos: 

- Não foi realizado teste de lotação da fila de mensagens ou de fluxos grandes de mensagens.

Dificuldades de implementação do modelo de threads:

- Perceber a necessidade de "esperar" o retorno de uma thread

