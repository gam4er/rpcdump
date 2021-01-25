/*
 * Copyright (c) BindView Development Corporation, 2001
 * See LICENSE file.
 * Author: Todd Sabin <tsabin@razor.bindview.com>
 */


#include <windows.h>
#include <winnt.h>

#include <stdio.h>

#include <rpc.h>
#include <rpcdce.h>

static int verbosity;


int
try_protocol (char *protocol, char *server)
{
    unsigned char *pStringBinding = NULL;
    RPC_BINDING_HANDLE hRpc;
    RPC_EP_INQ_HANDLE hInq;
    RPC_STATUS rpcErr;
    RPC_STATUS rpcErr2;
    int numFound = 0;

    //
    // Compose the string binding
    //
    rpcErr = RpcStringBindingCompose (NULL, protocol, server,
                                      NULL, NULL, &pStringBinding);
    if (rpcErr != RPC_S_OK) {
        fprintf (stderr, "RpcStringBindingCompose failed: %d\n", rpcErr);
        return numFound;
    }
    
    //
    // Convert to real binding
    //
    rpcErr = RpcBindingFromStringBinding (pStringBinding, &hRpc);
    if (rpcErr != RPC_S_OK) {
        fprintf (stderr, "RpcBindingFromStringBinding failed: %d\n", rpcErr);
        RpcStringFree (&pStringBinding);
        return numFound;
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
    "ncacn_ip_tcp",
    "ncadg_ip_udp",
    "ncacn_np",
    "ncacn_nb_tcp",
    "ncacn_http",
};
#define NUM_PROTOCOLS (sizeof (protocols) / sizeof (protocols[0]))

void
Usage (char *app)
{
    printf ("Usage: %s [options] <target>\n", app);
    printf ("  options:\n");
    printf ("    -p protseq   -- use protocol sequence\n", app);
    printf ("    -v           -- increase verbosity\n", app);
    exit (1);
}



int
main (int argc, char *argv[1])
{
    int i, j;
    char *target = NULL;
    char *protseq = NULL;

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
        for (i=0; i<NUM_PROTOCOLS; i++) {
            if (try_protocol (protocols[i], target) > 0) {
                break;
            }
        }
    }

    return 0;
}

