#include "parser.h"
#include "math_engine.h"
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LINE_BUF 4096

typedef enum {
    KW_TITLE, KW_RANGE, KW_POINT, KW_START, KW_MOVE, KW_VELOCITY, KW_SPEED,
    KW_COLOR, KW_LINE, KW_CHECK, KW_KEYPOINT, KW_OUTPUT, KW_AREA, KW_MARK,
    KW_CONGRUENT, KW_SIMILAR, KW_FUNCTION, KW_VECTOR, KW_MATRIX, KW_TABLE,
    KW_TEXT, KW_SOLVE, KW_REPEAT, KW_LOOP, KW_END_REPEAT, KW_END_LOOP,
    KW_LABEL, KW_MESSAGE, KW_RECORD, KW_NUMBER, KW_IMPORT, KW_REL, KW_FOR,
    KW_AS, KW_INF, KW_NONE
} Keyword;

static struct { const char* name; Keyword kw; } s_kwmap[] = {
    {"TITLE",KW_TITLE},{"RANGE",KW_RANGE},{"POINT",KW_POINT},{"START",KW_START},
    {"MOVE",KW_MOVE},{"VELOCITY",KW_VELOCITY},{"SPEED",KW_SPEED},{"COLOR",KW_COLOR},
    {"LINE",KW_LINE},{"CHECK",KW_CHECK},{"KEYPOINT",KW_KEYPOINT},{"OUTPUT",KW_OUTPUT},
    {"AREA",KW_AREA},{"MARK",KW_MARK},{"CONGRUENT",KW_CONGRUENT},{"SIMILAR",KW_SIMILAR},
    {"FUNCTION",KW_FUNCTION},{"VECTOR",KW_VECTOR},{"MATRIX",KW_MATRIX},{"TABLE",KW_TABLE},
    {"TEXT",KW_TEXT},{"SOLVE",KW_SOLVE},{"REPEAT",KW_REPEAT},{"LOOP",KW_LOOP},
    {"END_REPEAT",KW_END_REPEAT},{"END_LOOP",KW_END_LOOP},{"LABEL",KW_LABEL},
    {"MESSAGE",KW_MESSAGE},{"RECORD",KW_RECORD},{"NUMBER",KW_NUMBER},{"IMPORT",KW_IMPORT},
    {"REL",KW_REL},{"FOR",KW_FOR},{"AS",KW_AS},{"INF",KW_INF},{NULL,KW_NONE}
};

static Keyword kw_of(const char* s) {
    char u[64]; int i=0; while(s[i]&&i<63){u[i]=toupper((unsigned char)s[i]);i++;}u[i]=0;
    for(int j=0;s_kwmap[j].name;j++) if(strcmp(u,s_kwmap[j].name)==0) return s_kwmap[j].kw;
    return KW_NONE;
}

static void parse_color(const char* s, double c[4]) {
    c[0]=0.4;c[1]=0.4;c[2]=0.4;c[3]=1;
    if(!s||!*s)return;
    if(s[0]=='#'){
        unsigned r=0,g=0,b=0;
        size_t slen = strlen(s);
        if(slen>=7&&sscanf(s+1,"%2x%2x%2x",&r,&g,&b)==3){c[0]=r/255.0;c[1]=g/255.0;c[2]=b/255.0;}
        else if(slen==4){char buf[8];snprintf(buf,8,"#%c%c%c%c%c%c",s[1],s[1],s[2],s[2],s[3],s[3]);sscanf(buf+1,"%2x%2x%2x",&r,&g,&b);c[0]=r/255.0;c[1]=g/255.0;c[2]=b/255.0;}
    }
    else if(strncmp(s,"rgba",4)==0){double r,g,b,a;if(sscanf(s,"rgba(%lf,%lf,%lf,%lf)",&r,&g,&b,&a)==4){c[0]=r/255.0;c[1]=g/255.0;c[2]=b/255.0;c[3]=a;}}
}

static const double palette[8][4]={{0.91,0.30,0.24,1},{0.20,0.60,0.86,1},{0.18,0.80,0.44,1},{0.95,0.61,0.07,1},{0.61,0.35,0.71,1},{0.10,0.74,0.61,1},{0.90,0.37,0.00,1},{0.20,0.20,0.33,1}};
static int color_idx=0;
static void next_color(double c[4]){int ci=color_idx++%8;for(int i=0;i<4;i++)c[i]=palette[ci][i];}

// Tokenize rest of line: types: 0=num, 1=kw, 2=punct, 3=color, 4=id
static int tokenize(const char* rest, char tok[][64], int* typ, int max) {
    int n=0;const char*p=rest;
    while(*p&&n<max){
        while(*p&&isspace((unsigned char)*p))p++;
        if(!*p)break;
        if(*p==':'){p++;continue;} // skip colons (keyword-value separators)
        if(*p==';'||*p==','||*p=='('||*p==')'){tok[n][0]=*p;tok[n][1]=0;p++;typ[n]=2;n++;continue;}
        if(*p=='#'){if(*(p+1)&&isxdigit((unsigned char)*(p+1))){int i=0;tok[n][i++]='#';p++;while(*p&&isxdigit((unsigned char)*p)&&i<63)tok[n][i++]=*p,p++;tok[n][i]=0;typ[n]=3;n++;continue;}else break;}
        if(isdigit((unsigned char)*p)||(*p=='-'&&isdigit((unsigned char)*(p+1)))||*p=='.'){
            int i=0;if(*p=='-'){tok[n][i++]='-';p++;}while(*p&&(isdigit((unsigned char)*p)||*p=='.'||*p=='e'||*p=='E')){if((*p=='-'||*p=='+')&&i>0&&(tok[n][i-1]=='e'||tok[n][i-1]=='E'))tok[n][i++]=*p,p++;else if(*p=='e'||*p=='E'||*p=='.'||isdigit((unsigned char)*p))tok[n][i++]=*p,p++;else break;}
            tok[n][i]=0;typ[n]=0;n++;continue;
        }
        if(isalpha((unsigned char)*p)||*p=='_'){
            int i=0;while(*p&&(isalnum((unsigned char)*p)||*p=='_'||(unsigned char)*p>127))tok[n][i++]=*p,p++;
            tok[n][i]=0;
            // If followed by '(' — read until matching ')' as a single token (rgba, hsl, etc.)
            if(*p=='('){int d=1;tok[n][i++]='(';p++;while(*p&&d>0&&i<63){if(*p=='(')d++;if(*p==')')d--;if(d>=0||*p==')')tok[n][i++]=*p;p++;}tok[n][i]=0;}
            typ[n]=kw_of(tok[n])!=KW_NONE?1:4;n++;continue;
        }
        p++;
    }
    return n;
}

static double pnum(const char* s) {
    double v=strtod(s,NULL);if(v==0&&s[0]!='0'&&s[0]!='.')return eval_expr(s);
    return v;
}

// Check if keyword is point-related (for flush logic)
static int is_pt_kw(const char* k) {
    return strcmp(k,"POINT")==0||strcmp(k,"START")==0||strcmp(k,"MOVE")==0||strcmp(k,"COLOR")==0||strcmp(k,"VELOCITY")==0||strcmp(k,"__INF_BEGIN__")==0||strcmp(k,"__INF_END__")==0;
}

int parse_dsl(const char* text, AnimationData* data) {
    memset(data,0,sizeof(*data));
    color_idx=0;
    strcpy(data->title,"数学教学可视化");
    data->xMin=-10;data->xMax=10;data->yMin=-10;data->yMax=10;
    data->totalTime=10;

    char (*raw)[LINE_BUF] = calloc(512, LINE_BUF);
    int nraw=0;
    if(!raw)return 0;
    const char*p=text;
    while(*p&&nraw<512){int i=0;while(*p&&*p!='\n'&&i<LINE_BUF-1)raw[nraw][i++]=*p,p++;raw[nraw][i]=0;nraw++;if(*p=='\n')p++;}

    char (*exp)[LINE_BUF] = calloc(2048, LINE_BUF);
    int nexp=0;
    if(!exp){free(raw);return 0;}
    for(int i=0;i<nraw;i++){
        char*ln=raw[i];char*s=ln;while(*s&&isspace((unsigned char)*s))s++;
        if(!*s||*s=='#')continue;
        char kw[64];int kl=0;while(s[kl]&&!isspace((unsigned char)s[kl])&&s[kl]!=':'&&kl<63){kw[kl]=toupper((unsigned char)s[kl]);kl++;}kw[kl]=0;
        if(strcmp(kw,"REPEAT")==0||strcmp(kw,"LOOP")==0){
            const char*rp=s+kl;while(*rp&&*rp!=':')rp++;if(*rp==':')rp++;while(*rp&&isspace((unsigned char)*rp))rp++;
            int inf=(strncasecmp(rp,"INF",3)==0);int cnt=inf?0:atoi(rp);
            char (*body)[LINE_BUF] = calloc(512, LINE_BUF);int nb=0;i++;
            while(i<nraw&&nb<512){
                char*bl=raw[i];char*bs=bl;while(*bs&&isspace((unsigned char)*bs))bs++;
                if(strncasecmp(bs,"END_REPEAT",10)==0||strncasecmp(bs,"END_LOOP",8)==0){i++;break;}
                strcpy(body[nb++],bl);i++;
            }i--;
            if(inf){strcpy(exp[nexp++],"__INF_BEGIN__");for(int b=0;b<nb;b++)strcpy(exp[nexp++],body[b]);strcpy(exp[nexp++],"__INF_END__");}
            else for(int r=0;r<cnt;r++)for(int b=0;b<nb;b++)strcpy(exp[nexp++],body[b]);
            free(body);
        } else strcpy(exp[nexp++],s);
    }

    Point cur; int hasCur=0;



    for(int si=0;si<nexp;si++){
        char*s=exp[si];while(*s&&isspace((unsigned char)*s))s++;
        int colon=-1;for(int ci=0;s[ci];ci++)if(s[ci]==':'){colon=ci;break;}
        if(colon<0)continue;

        char kw[64];int kl=0;while(kl<colon&&kl<63){kw[kl]=toupper((unsigned char)s[kl]);kl++;}kw[kl]=0;
        const char*rest=s+colon+1;while(*rest&&isspace((unsigned char)*rest))rest++;

        char tok[64][64];int typ[64];
        int nt=tokenize(rest,tok,typ,64);
        double c[4];parse_color(NULL,c); // default gray

        if(strcmp(kw,"TITLE")==0){strncpy(data->title,rest,MAX_EXPR-1);}
        else if(strcmp(kw,"RANGE")==0&&nt>=4){data->xMin=atof(tok[0]);data->xMax=atof(tok[1]);data->yMin=atof(tok[2]);data->yMax=atof(tok[3]);}
        else         if(strcmp(kw,"POINT")==0){
            // Flush previous point when encountering a new POINT
            if(hasCur){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}
            memset(&cur,0,sizeof(cur));
            char label[64]="";int labelSet=0;
            double sx=0,sy=0;int hasStart=0;
            double ptColor[4];next_color(ptColor);
            for(int ti=0;ti<nt;ti++){
                if(typ[ti]==1&&strcasecmp(tok[ti],"START")==0){if(ti+2<nt&&typ[ti+1]==0&&typ[ti+2]==0){sx=atof(tok[ti+1]);sy=atof(tok[ti+2]);hasStart=1;ti+=2;}}
                else if(typ[ti]==1&&strcasecmp(tok[ti],"COLOR")==0&&ti+1<nt){parse_color(tok[ti+1],ptColor);ti++;}
                else if(typ[ti]==1&&strcasecmp(tok[ti],"MOVE")==0){
                    if(ti+2<nt&&typ[ti+1]==0&&typ[ti+2]==0){
                        double spd=1;
                        for(int sj=ti+3;sj<nt-1;sj++)if(typ[sj]==1&&strcasecmp(tok[sj],"SPEED")==0&&typ[sj+1]==0){spd=atof(tok[sj+1]);break;}
                        if(cur.numSegments<MAX_SEGMENTS){cur.segments[cur.numSegments].endX=atof(tok[ti+1]);cur.segments[cur.numSegments].endY=atof(tok[ti+2]);cur.segments[cur.numSegments].speed=spd;cur.numSegments++;}
                    }
                }
                else if(typ[ti]==1&&strcasecmp(tok[ti],"VELOCITY")==0&&ti+2<nt&&typ[ti+1]==0&&typ[ti+2]==0){
                    double vx=atof(tok[ti+1]),vy=atof(tok[ti+2]);
                    if(cur.numSegments<MAX_SEGMENTS){cur.segments[cur.numSegments].vx=vx;cur.segments[cur.numSegments].vy=vy;cur.segments[cur.numSegments].speed=hypot(vx,vy);cur.segments[cur.numSegments].isVelocity=1;cur.numSegments++;}
                    ti+=2;
                }
                else if(typ[ti]==4&&!labelSet){strncpy(label,tok[ti],63);labelSet=1;}
            }
            strncpy(cur.label,label[0]?label:"P",MAX_LABEL-1);
            cur.startX=sx;cur.startY=sy;
            for(int i=0;i<4;i++)cur.color[i]=ptColor[i];
            hasCur=1;
        }
        else if(strcmp(kw,"START")==0&&hasCur&&nt>=2){cur.startX=atof(tok[0]);cur.startY=atof(tok[1]);}
        else if(strcmp(kw,"MOVE")==0&&hasCur){
            int isRel=0;int ti0=0;
            if(nt>0&&strcasecmp(tok[0],"REL")==0){isRel=1;ti0=1;}
            double spd=1;
            double coords[2];int nc=0;
            for(int ti=ti0;ti<nt&&nc<2;ti++){
                if(typ[ti]==1&&strcasecmp(tok[ti],"SPEED")==0&&ti+1<nt){spd=atof(tok[ti+1]);ti++;continue;}
                if(typ[ti]==0){coords[nc++]=atof(tok[ti]);}
                else if(typ[ti]==4){
                    for(int pi=0;pi<data->numPoints;pi++)if(strcmp(data->points[pi].label,tok[ti])==0){coords[nc++]=data->points[pi].startX;if(nc<2)coords[nc++]=data->points[pi].startY;break;}
                }
            }
            if(nc>=2){
                double ex=coords[0],ey=coords[1];
                if(isRel){
                    double refX=cur.startX,refY=cur.startY;
                    if(cur.numSegments>0){refX=cur.segments[cur.numSegments-1].endX;refY=cur.segments[cur.numSegments-1].endY;}
                    ex+=refX;ey+=refY;
                }
                if(cur.numSegments<MAX_SEGMENTS){cur.segments[cur.numSegments].endX=ex;cur.segments[cur.numSegments].endY=ey;cur.segments[cur.numSegments].speed=spd;cur.numSegments++;}
            }
        }
        else if(strcmp(kw,"__INF_BEGIN__")==0&&hasCur){
            cur.numLoopBody=0;
            double lx=cur.startX,ly=cur.startY;
            if(cur.numSegments>0){lx=cur.segments[cur.numSegments-1].endX;ly=cur.segments[cur.numSegments-1].endY;}
            si++;
            while(si<nexp&&strcmp(exp[si],"__INF_END__")!=0){
                char*ls=exp[si];while(*ls&&isspace((unsigned char)*ls))ls++;
                if(strncasecmp(ls,"MOVE",4)==0){
                    const char*col=strchr(ls,':');if(!col){si++;continue;}col++;
                    char mt[64][64];int mtp[64];int mnt=tokenize(col,mt,mtp,64);
                    int rel=0;int mti=0;double mspd=1;
                    if(mnt>0&&strcasecmp(mt[0],"REL")==0){rel=1;mti=1;}
                    double mc[2];int mnc=0;
                    for(;mti<mnt&&mnc<2;mti++){
                        if(mtp[mti]==1&&strcasecmp(mt[mti],"SPEED")==0&&mti+1<mnt){mspd=atof(mt[++mti]);continue;}
                        if(mtp[mti]==0){mc[mnc++]=atof(mt[mti]);}
                    }
                    if(mnc>=2){
                        double mex=rel?lx+mc[0]:mc[0],mey=rel?ly+mc[1]:mc[1];
                        if(cur.numLoopBody<MAX_SEGMENTS){cur.loopBody[cur.numLoopBody].endX=mex;cur.loopBody[cur.numLoopBody].endY=mey;cur.loopBody[cur.numLoopBody].speed=mspd;cur.numLoopBody++;}
                        lx=mex;ly=mey;
                    }
                }
                si++;
            }
            double lt=0,lcx=cur.startX,lcy=cur.startY;
            if(cur.numSegments>0){lcx=cur.segments[cur.numSegments-1].endX;lcy=cur.segments[cur.numSegments-1].endY;}
            for(int j=0;j<cur.numLoopBody;j++){double dx=cur.loopBody[j].endX-lcx,dy=cur.loopBody[j].endY-lcy;double d=hypot(dx,dy);if(cur.loopBody[j].speed>0)lt+=d/cur.loopBody[j].speed;lcx=cur.loopBody[j].endX;lcy=cur.loopBody[j].endY;}
            cur.loopDuration=lt;
        }
        else if(strcmp(kw,"VELOCITY")==0&&hasCur&&nt>=2){if(cur.numSegments<MAX_SEGMENTS){cur.segments[cur.numSegments].vx=atof(tok[0]);cur.segments[cur.numSegments].vy=atof(tok[1]);cur.segments[cur.numSegments].speed=hypot(atof(tok[0]),atof(tok[1]));cur.segments[cur.numSegments].isVelocity=1;cur.numSegments++;}}
        else if(strcmp(kw,"COLOR")==0&&hasCur&&nt>=1){parse_color(tok[0],cur.color);}
        else if(strcmp(kw,"LINE")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            char label[64]="";double lc[4];parse_color("#666",lc);
            char raw[8][64];int nraw=0;
            for(int ti=0;ti<nt;ti++){
                if(typ[ti]==1&&strcasecmp(tok[ti],"LABEL")==0&&ti+1<nt){strncpy(label,tok[ti+1],63);ti++;}
                else if(typ[ti]==1&&strcasecmp(tok[ti],"COLOR")==0&&ti+1<nt){parse_color(tok[ti+1],lc);ti++;}
                else if(nraw<8)strncpy(raw[nraw++],tok[ti],63);
            }
            if(nraw>=2&&data->numLines<MAX_LINES){
                double vals[4];int nv=0;
                for(int ri=0;ri<nraw&&nv<4;ri++){
                    double v=atof(raw[ri]);
                    if(v!=0||raw[ri][0]=='0'||raw[ri][0]=='.'){vals[nv++]=v;}
                    else{                for(int pi=0;pi<data->numPoints;pi++)if(strcmp(data->points[pi].label,raw[ri])==0){vals[nv++]=data->points[pi].startX;vals[nv++]=data->points[pi].startY;break;}}
                }
                if(nv>=4){Line*l=&data->lines[data->numLines++];l->x1=vals[0];l->y1=vals[1];l->x2=vals[2];l->y2=vals[3];strcpy(l->label,label);for(int i=0;i<4;i++)l->color[i]=lc[i];}
            }
        }
        else if(strcmp(kw,"CHECK")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            const char*msg=strstr(rest,"MESSAGE");
            const char*rec=strstr(rest,"RECORD");
            char cond[256]="";char msgBuf[256]="点击继续";char recBuf[8][64];int nrec=0;
            const char*condEnd=rest+strlen(rest);
            if(msg){condEnd=msg;while(condEnd>rest&&isspace((unsigned char)condEnd[-1]))condEnd--;}
            if(rec&&rec<condEnd)condEnd=rec;
            int cl=(int)(condEnd-rest);if(cl>255)cl=255;strncpy(cond,rest,cl);cond[cl]=0;
            if(msg){const char*ms=msg+7;while(*ms&&isspace((unsigned char)*ms))ms++;const char*me=ms+strlen(ms);while(me>ms&&isspace((unsigned char)me[-1]))me--;int ml=(int)(me-ms);if(ml>255)ml=255;strncpy(msgBuf,ms,ml);msgBuf[ml]=0;}
            if(rec){const char*rs=rec+6;while(*rs&&isspace((unsigned char)*rs))rs++;char rb[256];int ri=0;while(*rs&&*rs!='M'&&ri<255)rb[ri++]=*rs++,rs--;rb[ri]=0;
                char*tk=strtok(rb,",");while(tk&&nrec<8){while(*tk&&isspace((unsigned char)*tk))tk++;strncpy(recBuf[nrec++],tk,63);tk=strtok(NULL,",");}}
            if(data->numChecks<MAX_CHECKS){Check*ch=&data->checks[data->numChecks++];strncpy(ch->condition,cond,MAX_EXPR-1);strncpy(ch->message,msgBuf,MAX_EXPR-1);for(int i=0;i<nrec;i++)strncpy(ch->recordExprs[i],recBuf[i],MAX_EXPR-1);ch->numRecordExprs=nrec;ch->triggerTime=-1;ch->triggered=0;}
        }
        else if(strcmp(kw,"OUTPUT")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            int typeLen=0;const char*typeStart=NULL;
            if(strncasecmp(rest,"length",6)==0){typeLen=6;typeStart="length";}
            else if(strncasecmp(rest,"angle",5)==0){typeLen=5;typeStart="angle";}
            else if(strncasecmp(rest,"area",4)==0){typeLen=4;typeStart="area";}
            else if(strncasecmp(rest,"perimeter",9)==0){typeLen=9;typeStart="perimeter";}
            else if(strncasecmp(rest,"judge",5)==0){typeLen=5;typeStart="judge";}
            if(!typeStart)continue;
            const char*coordStart=rest+typeLen;while(*coordStart&&isspace((unsigned char)*coordStart))coordStart++;
            char label[64]="";double oc[4];parse_color("#333",oc);
            char raw[8][64];int nraw=0;char judgeType[64]="";
            // Re-tokenize
            int nto=tokenize(coordStart,tok,typ,64);
            for(int ti=0;ti<nto;ti++){
                if(typ[ti]==1&&strcasecmp(tok[ti],"LABEL")==0&&ti+1<nto){strncpy(label,tok[ti+1],63);ti++;}
                else if(typ[ti]==1&&strcasecmp(tok[ti],"COLOR")==0&&ti+1<nto){parse_color(tok[ti+1],oc);ti++;}
                else if(nraw<8)strncpy(raw[nraw++],tok[ti],63);
            }
            if(strcmp(typeStart,"judge")==0&&nraw>0){strncpy(judgeType,raw[0],63);
                int shifted=0;char shiftRaw[8][64];
                for(int ri=1;ri<nraw;ri++){strncpy(shiftRaw[shifted++],raw[ri],63);}nraw=shifted;for(int ri=0;ri<shifted;ri++)strncpy(raw[ri],shiftRaw[ri],63);
            }
            if(data->numOutputs<MAX_OUTPUTS){
                Output*o=&data->outputs[data->numOutputs++];
                strncpy(o->type,typeStart,MAX_LABEL-1);
                strncpy(o->judgeType,judgeType,MAX_LABEL-1);
                strncpy(o->label,label[0]?label:typeStart,MAX_LABEL-1);
                for(int i=0;i<4;i++)o->color[i]=oc[i];
                o->numLabels=nraw;o->showOnCanvas=1;
                int hasNum=0;
                for(int i=0;i<nraw;i++){strncpy(o->labels[i],raw[i],MAX_LABEL-1);if(atof(raw[i])!=0||raw[i][0]=='0'||raw[i][0]=='.')hasNum=1;}
                if(hasNum){o->isNumeric=1;for(int i=0;i<nraw;i++)o->numeric[i]=atof(raw[i]);o->numNumeric=nraw;}
            }
        }
        else if(strcmp(kw,"AREA")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            const char*shapes[]={"triangle","rect","circle","square","trapezoid","parallelogram",NULL};
            int sh=-1;const char*sp=rest;
            for(int si2=0;shapes[si2];si2++){int l=strlen(shapes[si2]);if(strncasecmp(rest,shapes[si2],l)==0&&isspace((unsigned char)rest[l])){sh=si2;sp=rest+l;break;}}
            if(sh<0)continue;
            while(*sp&&isspace((unsigned char)*sp))sp++;
            int nat=tokenize(sp,tok,typ,64);
            char label[64]="S";double ac[4];parse_color("rgba(100,149,237,0.25)",ac);
            char raw[8][64];int nraw=0;
            for(int ti=0;ti<nat;ti++){
                if(typ[ti]==1&&strcasecmp(tok[ti],"LABEL")==0&&ti+1<nat){strncpy(label,tok[ti+1],63);ti++;}
                else if(typ[ti]==1&&strcasecmp(tok[ti],"COLOR")==0&&ti+1<nat){parse_color(tok[ti+1],ac);ti++;}
                else if(nraw<8)strncpy(raw[nraw++],tok[ti],63);
            }
            if(data->numAreas<MAX_AREAS){
                Area*a=&data->areas[data->numAreas++];
                strncpy(a->type,shapes[sh],MAX_LABEL-1);
                strncpy(a->label,label,MAX_LABEL-1);
                for(int i=0;i<4;i++)a->color[i]=ac[i];
                a->numPts=0;
                // Resolve points
                if(sh==0){ // triangle
                    if(nraw>=3){for(int ri=0;ri<3;ri++){strncpy(a->vertexLabels[ri],raw[ri],MAX_LABEL-1);int found=0;for(int pi=0;pi<data->numPoints;pi++)if(strcmp(data->points[pi].label,raw[ri])==0){a->pts[a->numPts++]=(Vec2){data->points[pi].startX,data->points[pi].startY};found=1;break;}if(!found){double v=atof(raw[ri]);a->pts[a->numPts++]=(Vec2){v,ri+1<nraw?atof(raw[ri+1]):0};if(ri+1<nraw)ri++;}}}
                }
                if(a->numPts>=3)a->value=compute_polygon_area(a->pts,a->numPts);
            }
        }
        else if(strcmp(kw,"MARK")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            int mt=-1;
            if(strncasecmp(rest,"angle",5)==0&&isspace((unsigned char)rest[5]))mt=0;
            else if(strncasecmp(rest,"right-angle",11)==0&&isspace((unsigned char)rest[11]))mt=1;
            else if(strncasecmp(rest,"segment",7)==0&&isspace((unsigned char)rest[7]))mt=2;
            else if(strncasecmp(rest,"parallel",8)==0&&isspace((unsigned char)rest[8]))mt=3;
            if(mt<0)continue;
            const char*sp2=rest+(mt==1?11:mt==0?5:mt==2?7:8);
            while(*sp2&&isspace((unsigned char)*sp2))sp2++;
            int nmt=tokenize(sp2,tok,typ,64);
            int num=0;double mc2[4];parse_color("#555",mc2);
            char raw2[8][64];int nraw2=0;
            for(int ti=0;ti<nmt;ti++){
                if(typ[ti]==1&&strcasecmp(tok[ti],"COLOR")==0&&ti+1<nmt){parse_color(tok[ti+1],mc2);ti++;}
                else if(typ[ti]==1&&strcasecmp(tok[ti],"NUMBER")==0&&ti+1<nmt){num=(int)atof(tok[ti+1]);ti++;}
                else strncpy(raw2[nraw2++],tok[ti],63);
            }
            if(data->numMarks<MAX_MARKS){
                Mark*m=&data->marks[data->numMarks++];
                const char*mnames[]={"angle","right-angle","segment","parallel"};
                strncpy(m->type,mnames[mt],MAX_LABEL-1);
                for(int i=0;i<4;i++)m->color[i]=mc2[i];
                m->number=num;
                int exp=mt==2?2:mt==3?4:3;
                for(int i=0;i<exp&&i<nraw2;i++)strncpy(m->labels[i],raw2[i],MAX_LABEL-1);
            }
        }
        else if(strcmp(kw,"CONGRUENT")==0||strcmp(kw,"SIMILAR")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            if(nt>=6&&data->numCongruents<8){
                Congruent*cg=&data->congruents[data->numCongruents++];
                strncpy(cg->type,strcmp(kw,"CONGRUENT")==0?"congruent":"similar",MAX_LABEL-1);
                for(int i=0;i<3;i++)strncpy(cg->tri1[i],tok[i],MAX_LABEL-1);
                for(int i=0;i<3;i++)strncpy(cg->tri2[i],tok[i+3],MAX_LABEL-1);
            }
        }
        else if(strcmp(kw,"KEYPOINT")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            if(nt>=2&&data->numKeypoints<MAX_KEYPOINTS){
                data->keypoints[data->numKeypoints].time=atof(tok[0]);
                // rest is label
                const char*kl=rest;while(*kl&&!isspace((unsigned char)*kl))kl++;while(*kl&&isspace((unsigned char)*kl))kl++;
                strncpy(data->keypoints[data->numKeypoints].label,kl,MAX_LABEL-1);
                data->numKeypoints++;
            }
        }
        else if(strcmp(kw,"FUNCTION")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            double fc[4];parse_color("#e74c3c",fc);
            char expr[256]="",fname[64]="",params[4][64];int np=0;
            // Find color
            const char*fp=rest;char fr[256];strncpy(fr,rest,255);
            char*colp=strstr(fr,"COLOR");if(colp){*colp=0;const char*cv=colp+5;while(*cv&&isspace((unsigned char)*cv))cv++;if(*cv==':')cv++;while(*cv&&isspace((unsigned char)*cv))cv++;parse_color(cv,fc);}
            char*eq=strchr(fr,'=');if(!eq)eq=strchr(fr,':');
            if(eq){
                // Find name and params before '='
                const char*npstart=fr;while(*npstart&&isspace((unsigned char)*npstart))npstart++;
                const char*paren=strchr(npstart,'(');
                if(paren&&paren<eq){
                    int nl=(int)(paren-npstart);if(nl>63)nl=63;strncpy(fname,npstart,nl);fname[nl]=0;
                    const char*pe=paren+1;const char*pc=strchr(pe,')');
                    if(pc){char pb[128];int pl=(int)(pc-pe);if(pl>127)pl=127;strncpy(pb,pe,pl);pb[pl]=0;char*tk=strtok(pb,",");while(tk&&np<4){while(*tk&&isspace((unsigned char)*tk))tk++;strncpy(params[np++],tk,63);tk=strtok(NULL,",");}}
                    eq++;while(*eq&&isspace((unsigned char)*eq))eq++;
                    strncpy(expr,eq,255);
                }
            }
            if(fname[0]&&data->numFunctions<MAX_FUNCTIONS){
                Function*f=&data->functions[data->numFunctions++];
                strncpy(f->fname,fname,MAX_LABEL-1);
                for(int i=0;i<np;i++)strncpy(f->params[i],params[i],MAX_LABEL-1);f->numParams=np;
                strncpy(f->expr,expr,MAX_EXPR-1);
                for(int i=0;i<4;i++)f->color[i]=fc[i];
            }
        }
        else if(strcmp(kw,"VECTOR")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            char label[64]="v";double vc[4];parse_color("#3498db",vc);
            char raw[8][64];int nraw=0;
            for(int ti=0;ti<nt;ti++){
                if(typ[ti]==1&&strcasecmp(tok[ti],"LABEL")==0&&ti+1<nt){strncpy(label,tok[ti+1],63);ti++;}
                else if(typ[ti]==1&&strcasecmp(tok[ti],"COLOR")==0&&ti+1<nt){parse_color(tok[ti+1],vc);ti++;}
                else if(nraw<8)strncpy(raw[nraw++],tok[ti],63);
            }
            if(data->numVectors<MAX_VECTORS){
                Vector*v=&data->vectors[data->numVectors++];
                strncpy(v->type,"coords",MAX_LABEL-1);
                strncpy(v->label,label,MAX_LABEL-1);
                for(int i=0;i<4;i++)v->color[i]=vc[i];
                // Try to resolve
                double vals[4];int nv=0;
                for(int ri=0;ri<nraw&&nv<4;ri++){
                    double va=atof(raw[ri]);
                    if(va!=0||raw[ri][0]=='0'||raw[ri][0]=='.'){vals[nv++]=va;}
                    else{for(int pi=0;pi<data->numPoints;pi++)if(strcmp(data->points[pi].label,raw[ri])==0){vals[nv++]=data->points[pi].startX;if(ri+1<nraw&&nv<4){vals[nv++]=data->points[pi].startY;ri++;}break;}}
                }
                if(nv>=4){v->x1=vals[0];v->y1=vals[1];v->x2=vals[2];v->y2=vals[3];}
                else if(nv>=2){v->x1=0;v->y1=0;v->x2=vals[0];v->y2=vals[1];}
            }
        }
        else if(strcmp(kw,"MATRIX")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            char label[64]="M";
            for(int ti=0;ti<nt;ti++)if(typ[ti]==1&&strcasecmp(tok[ti],"LABEL")==0&&ti+1<nt){strncpy(label,tok[ti+1],63);break;}
            if(data->numMatrices<MAX_MATRICES){
                Matrix*m=&data->matrices[data->numMatrices++];
                strncpy(m->label,label,MAX_LABEL-1);m->rows=0;
                char rowBuf[1024];strncpy(rowBuf,rest,1023);
                char*row=strtok(rowBuf,";");while(row&&m->rows<8){
                    double*vals=m->data[m->rows];int nc=0;
                    char*c=row;while(*c&&nc<8){while(*c&&isspace((unsigned char)*c))c++;if(!*c||*c==';')break;vals[nc++]=atof(c);while(*c&&!isspace((unsigned char)*c)&&*c!=';')c++;}
                    if(nc>0){if(nc>m->cols)m->cols=nc;m->rows++;}row=strtok(NULL,";");
                }
            }
        }
        else if(strcmp(kw,"TABLE")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            char label[64]="";
            for(int ti=0;ti<nt;ti++)if(typ[ti]==1&&strcasecmp(tok[ti],"LABEL")==0&&ti+1<nt){strncpy(label,tok[ti+1],63);break;}
            if(data->numTables<MAX_TABLES){
                Table*t=&data->tables[data->numTables++];
                strncpy(t->label,label,MAX_LABEL-1);t->rows=0;
                char rowBuf[1024];strncpy(rowBuf,rest,1023);
                char*row=strtok(rowBuf,";");while(row&&t->rows<8){
                    double*vals=t->data[t->rows];int nc=0;
                    char*c=row;while(*c&&nc<8){while(*c&&isspace((unsigned char)*c))c++;if(!*c||*c==';')break;vals[nc++]=atof(c);while(*c&&!isspace((unsigned char)*c)&&*c!=';')c++;}
                    if(nc>0){if(nc>t->cols)t->cols=nc;t->rows++;}row=strtok(NULL,";");
                }
            }
        }
        else if(strcmp(kw,"TEXT")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            double tc[4];parse_color("#333",tc);
            const char*col=strstr(rest,"COLOR");
            char txt[256];int txtLen;
            if(col){txtLen=(int)(col-rest);while(txtLen>0&&isspace((unsigned char)rest[txtLen-1]))txtLen--;strncpy(txt,rest,txtLen);txt[txtLen]=0;const char*cv=col+5;while(*cv&&*cv!=':')cv++;if(*cv==':')cv++;while(*cv&&isspace((unsigned char)*cv))cv++;parse_color(cv,tc);}
            else{strncpy(txt,rest,255);}
            if(txt[0]&&data->numTexts<MAX_TEXTS){Text*t=&data->texts[data->numTexts++];strncpy(t->text,txt,MAX_EXPR-1);for(int i=0;i<4;i++)t->color[i]=tc[i];}
        }
        else if(strcmp(kw,"SOLVE")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            char var[64]="x",expr[256]="";double from=-10,to=10;
            const char*fp=strstr(rest,"FOR");if(fp){fp+=3;while(*fp&&*fp!=':')fp++;if(*fp==':')fp++;while(*fp&&isspace((unsigned char)*fp))fp++;int vi=0;while(fp[vi]&&!isspace((unsigned char)fp[vi])&&fp[vi]!=':'&&vi<63){var[vi]=fp[vi];vi++;}var[vi]=0;}
            const char*rp=strstr(rest,"RANGE");if(rp){rp+=5;while(*rp&&*rp!=':')rp++;if(*rp==':')rp++;from=atof(rp);while(*rp&&!isspace((unsigned char)*rp))rp++;while(*rp&&isspace((unsigned char)*rp))rp++;to=atof(rp);}
            const char*eqp=strstr(rest,"=");if(eqp){const char*es=eqp+1;while(*es&&isspace((unsigned char)*es))es++;strncpy(expr,es,255);}
            if(!expr[0])strncpy(expr,rest,255);
            if(data->numFunctions<MAX_FUNCTIONS){
                Function*f=&data->functions[data->numFunctions++];
                strcpy(f->fname,"solve");strcpy(f->params[0],var);f->numParams=1;
                strncpy(f->expr,expr,MAX_EXPR-1);
                for(int i=0;i<4;i++)f->color[i]=(double[]){0.61,0.35,0.71,1}[i];
                f->isSolve=1;
            }
        }
        else if(strcmp(kw,"IMPORT")==0){
            do{if(hasCur&&!is_pt_kw(kw)){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}}while(0);
            if(nt>=1&&data->numImports<MAX_IMPORTS){
                Import*im=&data->imports[data->numImports++];
                strncpy(im->name,tok[0],MAX_LABEL-1);
                for(int ti=1;ti<nt-1;ti++)if(strcasecmp(tok[ti],"AS")==0){strncpy(im->alias,tok[ti+1],MAX_LABEL-1);break;}
            }
        }
    }
    // Final flush: push any remaining point
    if(hasCur){if(data->numPoints<MAX_POINTS)data->points[data->numPoints++]=cur;hasCur=0;}

    // Compute times
    double maxTime=0;int hasInf=0;
    for(int i=0;i<data->numPoints;i++){
        Point*pt=&data->points[i];double t=0,cx=pt->startX,cy=pt->startY;
        for(int j=0;j<pt->numSegments;j++){
            if(pt->segments[j].isVelocity)continue;
            double dx=pt->segments[j].endX-cx,dy=pt->segments[j].endY-cy,d=hypot(dx,dy);
            if(pt->segments[j].speed>0)t+=d/pt->segments[j].speed;
            cx=pt->segments[j].endX;cy=pt->segments[j].endY;
        }
        if(pt->numLoopBody>0&&pt->loopDuration>0){hasInf=1;t+=pt->loopDuration;}
        if(t>maxTime)maxTime=t;
    }
    data->totalTime=maxTime>0?maxTime:10;
    data->hasInfiniteLoop=hasInf;

    free(raw); free(exp);
    return 1;
}
