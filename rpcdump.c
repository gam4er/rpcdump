#define WIN32_LEAN_AND_MEAN

#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)  
#define bcopy(b1,b2,len) (memmove((b2), (b1), (len)), (void) 0)

#include <windows.h>
#include <winnt.h>

#include <stdio.h>

#include <rpc.h>
#include <rpcdce.h>

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <winsock2.h>
#include <ws2tcpip.h>




// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

static int verbosity;

BOOL fastconnect(char* ipaddr, int port)
{
    // you really shouldn't be calling WSAStartup() here.
    // Call it at app startup instead...


    struct sockaddr_in server = { 0 };
    
    server.sin_family = AF_INET;       

    server.sin_addr.s_addr = inet_addr(ipaddr);
    server.sin_port = htons(port);

    int sock;

    // ipaddr valid?
    if (server.sin_addr.s_addr == INADDR_NONE)
        return FALSE;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return FALSE;

    // put socked in non-blocking mode...
    u_long block = 1;
    if (ioctlsocket(sock, FIONBIO, &block) == SOCKET_ERROR)
    {
        closesocket(sock);
        //closesock(sock);
        return FALSE;
    }

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
            // connection failed
            closesocket(sock);
            //closesock(sock);
            return FALSE;
        }

        // connection pending

        fd_set setW, setE;

        FD_ZERO(&setW);
        FD_SET(sock, &setW);
        FD_ZERO(&setE);
        FD_SET(sock, &setE);

        TIMEVAL time_out /*= { 0 }*/;
        time_out.tv_sec = 0;
        time_out.tv_usec = 30000;

        int ret = select(0, NULL, &setW, &setE, &time_out);
        if (ret <= 0)
        {
            // select() failed or connection timed out
            closesocket(sock);
            //closesock(sock);
            if (ret == 0)
                WSASetLastError(WSAETIMEDOUT);
            return FALSE;
        }

        if (FD_ISSET(sock, &setE))
        {
            // connection failed            
            int err = 0;
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, sizeof(err));
            closesocket(sock);
            //closesock(sock);
            WSASetLastError(err);
            return FALSE;
        }
    }

    // connection successful
    // put socked in blocking mode...
    block = 0;
    if (ioctlsocket(sock, FIONBIO, &block) == SOCKET_ERROR)
    {
        closesocket(sock);
        //closesock(sock);
        return FALSE;
    }

    closesocket(sock);
    return TRUE;
}

int try_protocol (char *protocol, char *server)
{
    unsigned char *pStringBinding = NULL;
    RPC_BINDING_HANDLE hRpc;
    RPC_EP_INQ_HANDLE hInq;
    RPC_STATUS rpcErr;
    RPC_STATUS rpcErr2;
    int numFound = 0;
    /*
    BOOL res;
    res = fastconnect(server, 135);
       
    if (res == 0)
    {
        res = fastconnect(server, 445);
        if (res == 0)
        {
            printf("RPC on %s are disabled\n", server);
            return numFound;
        }
    }
    */
    printf("RPC on %s using %s\n", server, protocol);

    //
    // Compose the string binding
    //
    rpcErr = RpcStringBindingCompose (NULL, protocol, server,
                                      NULL, NULL, &pStringBinding);
    if (rpcErr != RPC_S_OK) {
        fprintf (stderr, "RpcStringBindingCompose failed: %d using %s protocol\n", rpcErr, protocol);
        return numFound;
    }

    //
    // Convert to real binding
    //
    rpcErr = RpcBindingFromStringBinding (pStringBinding, &hRpc);
    if (rpcErr != RPC_S_OK) {
        fprintf (stderr, "RpcBindingFromStringBinding failed: %d using %s protocol\n", rpcErr, protocol);
        RpcStringFree (&pStringBinding);
        return numFound;
    }

    rpcErr = RpcBindingSetOption(&hRpc, RPC_C_OPT_CALL_TIMEOUT,3);
    if (rpcErr != RPC_S_OK) {
        fprintf(stderr, "RpcBindingSetOption failed: %d\n", rpcErr);
    }

    //
    // Begin Ep enum
    //
    rpcErr = RpcMgmtEpEltInqBegin (hRpc, RPC_C_EP_ALL_ELTS, NULL, 0,
                                   NULL, &hInq);
    if (rpcErr != RPC_S_OK) {
        fprintf (stderr, "RpcMgmtEpEltInqBegin failed: %d\n", rpcErr);
        RpcStringFree (&pStringBinding);
        RpcBindingFree (hRpc);
        return numFound;
    }

    //
    // While Next succeeds
    //
    do {
        RPC_IF_ID IfId;
        RPC_IF_ID_VECTOR *pVector;
        RPC_STATS_VECTOR *pStats;
        RPC_BINDING_HANDLE hEnumBind;
        UUID uuid;
        unsigned char *pAnnot;

        rpcErr = RpcMgmtEpEltInqNext (hInq, &IfId, &hEnumBind, &uuid, &pAnnot);
        if (rpcErr == RPC_S_OK) {
            unsigned char *str = NULL;
            unsigned char *princName = NULL;
            numFound++;

            //
            // Print IfId
            //
            if (UuidToString (&IfId.Uuid, &str) == RPC_S_OK) {
                printf ("IfId: %s version %d.%d\n", str, IfId.VersMajor,
                        IfId.VersMinor);
                RpcStringFree (&str);
            }

            //
            // Print Annot
            //
            if (pAnnot) {
                printf ("Annotation: %s\n", pAnnot);
                RpcStringFree (&pAnnot);
            }

            //
            // Print object ID
            //
            if (UuidToString (&uuid, &str) == RPC_S_OK) {
                printf ("UUID: %s\n", str);
                RpcStringFree (&str);
            }

            //
            // Print Binding
            //
            if (RpcBindingToStringBinding (hEnumBind, &str) == RPC_S_OK) {
                printf ("Binding: %s\n", str);
                RpcStringFree (&str);
            }

            if (verbosity >= 1) {
                unsigned char *strBinding = NULL;
                unsigned char *strObj = NULL;
                unsigned char *strProtseq = NULL;
                unsigned char *strNetaddr = NULL;
                unsigned char *strEndpoint = NULL;
                unsigned char *strNetoptions = NULL;
                RPC_BINDING_HANDLE hIfidsBind;

                //
                // Ask the RPC server for its supported interfaces
                //
                //
                // Because some of the binding handles may refer to
                // the machine name, or a NAT'd address that we may
                // not be able to resolve/reach, parse the binding and
                // replace the network address with the one specified
                // from the command line.  Unfortunately, this won't
                // work for ncacn_nb_tcp bindings because the actual
                // NetBIOS name is required.  So special case those.
                //
                // Also, skip ncalrpc bindings, as they are not
                // reachable from a remote machine.
                //
                rpcErr2 = RpcBindingToStringBinding (hEnumBind, &strBinding);
                RpcBindingFree (hEnumBind);
                if (rpcErr2 != RPC_S_OK) {
                    fprintf (stderr, ("RpcBindingToStringBinding failed\n"));
                    printf ("\n");
                    continue;
                }

                if (strstr (strBinding, "ncalrpc") != NULL) {
                    RpcStringFree (&strBinding);
                    printf ("\n");
                    continue;
                }

                rpcErr2 = RpcStringBindingParse (strBinding, &strObj, &strProtseq,
                                                 &strNetaddr, &strEndpoint, &strNetoptions);
                RpcStringFree (&strBinding);
                strBinding = NULL;
                if (rpcErr2 != RPC_S_OK) {
                    fprintf (stderr, ("RpcStringBindingParse failed\n"));
                    printf ("\n");
                    continue;
                }

                rpcErr2 = RpcStringBindingCompose (strObj, strProtseq,
                                                   strcmp ("ncacn_nb_tcp", strProtseq) == 0 ? strNetaddr : server,
                                                   strEndpoint, strNetoptions,
                                                   &strBinding);
                RpcStringFree (&strObj);
                RpcStringFree (&strProtseq);
                RpcStringFree (&strNetaddr);
                RpcStringFree (&strEndpoint);
                RpcStringFree (&strNetoptions);
                if (rpcErr2 != RPC_S_OK) {
                    fprintf (stderr, ("RpcStringBindingCompose failed\n"));
                    printf ("\n");
                    continue;
                }

                rpcErr2 = RpcBindingFromStringBinding (strBinding, &hIfidsBind);
                RpcStringFree (&strBinding);
                if (rpcErr2 != RPC_S_OK) {
                    fprintf (stderr, ("RpcBindingFromStringBinding failed\n"));
                    printf ("\n");
                    continue;
                }

                if ((rpcErr2 = RpcMgmtInqIfIds (hIfidsBind, &pVector)) == RPC_S_OK) {
                    unsigned int i;
                    printf ("RpcMgmtInqIfIds succeeded\n");
                    printf ("Interfaces: %d\n", pVector->Count);
                    for (i=0; i<pVector->Count; i++) {
                        unsigned char *str = NULL;
                        UuidToString (&pVector->IfId[i]->Uuid, &str);
                        printf ("  %s v%d.%d\n", str ? str : "(null)",
                                pVector->IfId[i]->VersMajor,
                                pVector->IfId[i]->VersMinor);
                        if (str) RpcStringFree (&str);
                    }
                    RpcIfIdVectorFree (&pVector);
                } else {
                    printf ("RpcMgmtInqIfIds failed: 0x%x\n", rpcErr2);
                }

                if (verbosity >= 2) {
                    if ((rpcErr2 = RpcMgmtInqServerPrincName (hEnumBind,
                                                              RPC_C_AUTHN_WINNT,
                                                              &princName)) == RPC_S_OK) {
                        printf ("RpcMgmtInqServerPrincName succeeded\n");
                        printf ("Name: %s\n", princName);
                        RpcStringFree (&princName);
                    } else {
                        printf ("RpcMgmtInqServerPrincName failed: 0x%x\n", rpcErr2);
                    }

                    if ((rpcErr2 = RpcMgmtInqStats (hEnumBind,
                                                    &pStats)) == RPC_S_OK) {
                        unsigned int i;
                        printf ("RpcMgmtInqStats succeeded\n");
                        for (i=0; i<pStats->Count; i++) {
                            printf ("  Stats[%d]: %d\n", i, pStats->Stats[i]);
                        }
                        RpcMgmtStatsVectorFree (&pStats);
                    } else {
                        printf ("RpcMgmtInqStats failed: 0x%x\n", rpcErr2);
                    }
                }
                RpcBindingFree (hIfidsBind);
            }
            printf ("\n");
        }
    } while (rpcErr != RPC_X_NO_MORE_ENTRIES);

    //
    // Done
    //
    RpcStringFree (&pStringBinding);
    RpcBindingFree (hRpc);

    return numFound;
}


char *protocols[] = {
"ncacn_nb_tcp",
"ncacn_nb_ipx",
"ncacn_nb_nb",
"ncacn_ip_tcp",
"ncacn_np",
"ncacn_spx",
"ncacn_dnet_nsp",
"ncacn_at_dsp",
"ncacn_vns_spp",
"ncadg_mq",
"ncacn_http",
"ncadg_ip_udp",
"ncadg_ipx",
"ncalrpc",
};
#define NUM_PROTOCOLS (sizeof (protocols) / sizeof (protocols[0]))

void
Usage (char *app)
{
    printf ("Usage: %s [options] <target>\n", app);
    printf ("  options:\n");
    printf ("    -p protseq   -- use protocol sequence\n");
    printf ("    -v           -- increase verbosity\n");
    exit (1);
}



int
main (int argc, char *argv[1])
{
    int i, j;
    char *target = NULL;
    char *protseq = NULL;

    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);

    for (j=1; j<argc; j++) {
        if (argv[j][0] == '-') {
            switch (argv[j][1]) {

            case 'v':
                verbosity++;
                break;

            case 'p':
                protseq = argv[++j];
                break;

            default:
                Usage (argv[0]);
                break;
            }
        } else {
            target = argv[j];
        }
    }

    if (!target) {
        fprintf (stderr, "Usage: %s <server>\n", argv[0]);
        exit (1);
    }

    if (protseq) {
        try_protocol (protseq, target);
    } else {
        /*
        BOOL res;
        res = fastconnect(target, 135);

        if (res == 0)
        {
            res = fastconnect(target, 445);
            if (res == 0)
            {
                printf("RPC on %s are disabled\n", target);
                return 0;
            }
        }
        */
        for (i=0; i<NUM_PROTOCOLS; i++) {
            if (try_protocol (protocols[i], target) > 0) {
                //break;
            }
        }
    }

    WSACleanup();

    return 0;
}

