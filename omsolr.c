/* omsolr.c
 * This output plugin enables rsyslog to post to a Solr core instance.
 *
 * Copyright 2011 Donet, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: James Keating
 * <jamesk@donet.com>
 *
 */

#include <curl/curl.h>

#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

static uchar *url = NULL;

typedef struct _instanceData {
    uchar *url;	/* The full URL to the solr core instance, server:8080/solr/corename/ */
    CURL *curl;
    FILE *nullfile;
    struct curl_slist *slist;
} instanceData;



BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
    if(eFeat == sFEATURERepeatedMsgReduction)
        iRet = RS_RET_OK;
ENDisCompatibleWithFeature


static void closeCurl(instanceData *pData)
{
    ASSERT(pData != NULL);

    if(pData->curl != NULL) {   
        curl_easy_cleanup(pData->curl);
        pData->curl= NULL;
    }
}

BEGINfreeInstance
CODESTARTfreeInstance
    closeCurl(pData);
    fclose(pData->nullfile);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
ENDdbgPrintInstInfo

static rsRetVal initCurl(instanceData *pData, int bSilent) {

    DEFiRet;

    (void) bSilent;
    CURLcode content_rsp;
    CURLcode writedata_rsp;
    CURLcode url_rsp;
    CURLcode test_server;

    pData->nullfile = fopen("/dev/null", "w");

    ASSERT(pData != NULL);
    ASSERT(pData->curl == NULL);

    pData->curl = curl_easy_init();
    
    if (pData->curl == NULL) {
        errmsg.LogError(0, RS_RET_SUSPENDED, "curl error: failed to initialize curl - suspending.");
        closeCurl(pData);
        iRet = RS_RET_SUSPENDED;
    } else {
        pData->slist = curl_slist_append(NULL, "Content-Type: text/xml; charset=utf-8");
        content_rsp = curl_easy_setopt(pData->curl, CURLOPT_HTTPHEADER, pData->slist);
        if(content_rsp) {
            errmsg.LogError(0, RS_RET_SUSPENDED, "curl error: failed to set content-type.");
        	iRet = RS_RET_SUSPENDED;
    	} else {
            writedata_rsp = curl_easy_setopt(pData->curl, CURLOPT_WRITEDATA, pData->nullfile);
            if(writedata_rsp) {
                errmsg.LogError(0, RS_RET_SUSPENDED, "curl error: failed to set null output.");
            	iRet = RS_RET_SUSPENDED;
            } else {
                url_rsp = curl_easy_setopt(pData->curl, CURLOPT_URL, pData->url);
                if(url_rsp) {
                    errmsg.LogError(0, RS_RET_SUSPENDED, "curl error: failed to set url.");
                	iRet = RS_RET_SUSPENDED;
                } else {
                    test_server = curl_easy_perform(pData->curl);
                    if(test_server) {
                        errmsg.LogError(0, RS_RET_SUSPENDED, "curl error: server is unaccessible.");
                    	iRet = RS_RET_SUSPENDED;
                    }
                }
            }
        }
    }

    RETiRet;
}


BEGINtryResume
CODESTARTtryResume
if(pData->curl == NULL) {
    iRet = initCurl(pData, 1);
}
ENDtryResume


BEGINbeginTransaction
CODESTARTbeginTransaction
ENDbeginTransaction


BEGINendTransaction
CODESTARTendTransaction
ENDendTransaction


rsRetVal postSolr(uchar *psz, instanceData *pData)
{
    DEFiRet;
    
    CURLcode res;
    long http_resp = 0;

    ASSERT(psz != NULL);
    ASSERT(pData != NULL);

    /* see if we are ready to proceed */
    if(pData->curl == NULL) {
        CHKiRet(initCurl(pData, 0));
    }

    curl_easy_setopt(pData->curl, CURLOPT_POSTFIELDS, (const char*)psz);
    res = curl_easy_perform(pData->curl);
    
    if(!res) {
        curl_easy_getinfo(pData->curl, CURLINFO_RESPONSE_CODE, &http_resp);
        if (!http_resp == 200) {
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        } else {
            iRet = RS_RET_OK;
        }
    } else {
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

finalize_it:
    if(iRet == RS_RET_OK) {
        res = 0;
        http_resp = 0;
    }

    RETiRet;
}

BEGINdoAction
CODESTARTdoAction
    CHKiRet(postSolr(ppString[0], pData));
    if(bCoreSupportsBatching)
        iRet = RS_RET_DEFER_COMMIT;
finalize_it:
ENDdoAction


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
    if(strncmp((char*) p, ":omsolr:", sizeof(":omsolr:") - 1)) {
        ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    p += sizeof(":omsolr:") - 1;
    CHKiRet(createInstance(&pData));
    
    if (url == NULL) {
        ABORT_FINALIZE( RS_RET_PARAM_ERROR );
    } else {
        CHKmalloc(pData->url = (uchar*) strdup((char*)url));
    }

    if(*(p-1) == ';')
        --p;

    CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) "RSYSLOG_TraditionalForwardFormat"));

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_TXIF_OMOD_QUERIES
ENDqueryEtryPt


static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
    DEFiRet;
    if (url != NULL) {
        free(url);
        url = NULL;
    }

    RETiRet;
}


BEGINmodInit()
CODESTARTmodInit
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
    CHKiRet(objUse(errmsg, CORE_COMPONENT));
    INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionsolrurl", 0, eCmdHdlrGetWord, NULL, &url, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
