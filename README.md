# 2018 mini mmoRPG server proj

# 구성
클라이언트의 접속, 그 외의 네트워크 기능을 담당하는 
Network Class / DB 의 접근을 담당하는 DBManager Class로 나뉩니다.

# Network Class
Network Class 에서는 IOCP 소켓모델을 사용하여 서버를 구축하였고,
멀티스레드를 accept, worker, timer 로 나눴습니다.
각각은 접속, 연산, 타이머 처리 들에 사용됩니다.

중점적으로 볼 사항은 worker thread 입니다.

# worker thread
worker thread 에서는 패킷 타입에 따라 다른 동작을 수행하도록 합니다.
패킷 타입에는
RECV, SEND, MOVE, LOGIN이 있습니다.

GetQueuedCompletionStatus 함수를 통해 패킷이 넘어오면 검사합니다.

클라이언트에서 패킷을 RECV 타입으로 보내서 전송하게되면
worker thread 에서 재조립을 하고 process packet 함수로 보냅니다.

MOVE 타입은 NPC들이 움직일 경우 발생합니다. 
NPC들은 timer thread에 1초간 잠들어있다가 패킷타입을 MOVE로 바꾼후
PostQueuedCompletionStatus 함수에 의해 이벤트 발생으로 worker thread로 넘겨지게됩니다.

# process packet()
process packet 함수에서는 클라이언트에서 요청한 동작을 수행한 후 
다시 해당 클라이언트와 다른 클라이언트에 뿌려줍니다.
이동, 충돌, 공격등에 대한 처리들을 진행합니다.

process packet의 가장 중요한 점은 시야처리를 합니다.
접속자수가 많아지면 서로 통신해야할 클라이언트들이 많아지고 결국 필요없는 처리들(보이지않는 거리이지만 통신을 함)이 발생하기 때문에,
일정 범위를 정해서 view list를 생성하고 view list를 갱신해주면서 해당 view list 에 있는 클라이언트들에게만 처리후 정보를 보내줍니다.

viewlist 는 검색이 빠른 해시자료구조 이면서 중복이 되지 않는 unordered_set<> 자료구조를 사용했습니다.

# DBManager Class
DBManager는 mssql을 접근하는 class 입니다.
DBManager는 Search, Insert, Update의 기능을 수행합니다.

DBManager는 처음 프로그램이 시작되면 같이 생성되며, DB에 언제든지 접근할 수 있게 mssql에 연결됩니다.
workerthread에서 LOGIN 타입의 패킷이 넘어오면, 
DBManager를 통해 SQL query문을 조립해서 search 하고 
있을경우 DB에서 모든 정보를 받아와 Network Class에서 클라이언트 객체를 생성하고 넣어줍니다.
없을경우 insert 시켜서 DB에 새로운 데이터를 넣어주고, Network Class 에서 클라이언트 객체를 생성합니다.

클라이언트의 연결이 끊기면, 클라이언트 정보를 DB에 update하고 클라이언트 객체를 Network Class에서 삭제합니다.
