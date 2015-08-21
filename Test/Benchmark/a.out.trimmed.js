function(global,env,buffer) 
{
    "use asm";
    var a=new global.Int8Array(buffer);
    var b=new global.Int16Array(buffer);
    var c=new global.Int32Array(buffer);
    var d=new global.Uint8Array(buffer);
    var e=new global.Uint16Array(buffer);
    var f=new global.Uint32Array(buffer);
    var g=new global.Float32Array(buffer);
    var h=new global.Float64Array(buffer);
    var i=env.STACKTOP|0;
    var n=0;
    var o=0;
    var p=0;
    var q=0;
    var r=global.NaN,s=global.Infinity;
    var t=0,u=0,v=0,w=0,x=0.0,y=0,z=0,A=0,B=0.0;
    var C=0;
    var D=0;
    var E=0;
    var F=0;
    var G=0;
    var H=0;
    var I=0;
    var J=0;
    var K=0;
    var L=0;
    var _=global.Math.imul;
    var aa=global.Math.clz32;
    var ba=env.abort;
    var la=env.___cxa_throw;
    var na=env._abort;
    var pa=env._sbrk;
    var qa=env._time;
    var ta=env.___errno_location;
    var wa=env._sysconf;
    var xa=env.___cxa_allocate_exception;
    var za=0.0;

    // EMSCRIPTEN_START_FUNCS
    function Qa()
    {
        var a=0,b=0,d=0,e=0,f=0;
        d=Sa(4e8)|0;
        a=0;
        do
        {
            c[d+(a<<2)>>2]=nb()|0;
            a=a+1|0
        }
        while((a|0)!=1e8);
        a=0;
        do
        {
            f=d+(a<<2)|0;
            e=c[f>>2]|0;
            b=d+(99999999-a<<2)|0;
            c[f>>2]=c[b>>2];
            c[b>>2]=e;
            a=a+1|0
        }
        while((a|0)!=1e8);
        b=0;
        a=0;
        do
        {
            a=a-(c[d+(b<<2)>>2]|0)|0;
            b=b+1|0
        }
        while((b|0)!=1e8);
        return a|0
    }
    function Ra(a)
    {
        a=a|0;
        var b=0;
        b=(a|0)==0?1:a;
        a=ob(b)|0;
        a:do if(!a)
        {
            while(1)
            {
                a=Xa()|0;
                if(!a)break;
                Ea[a&0]();
                a=ob(b)|0;
                if(a)break a
            }
            b=xa(4)|0;
            c[b>>2]=16;
            la(b|0,48,1)
        }
        while(0);
        return a|0
    }
    function Sa(a)
    {
        a=a|0;
        return Ra(a)|0
    }
    function Ta(a)
    {
        a=a|0;
        pb(a);
        return
    }
    function Ua(a)
    {
        a=a|0;
        return
    }
    function Va(a)
    {
        a=a|0;
        Ta(a);
        return
    }
    function Wa(a)
    {
        a=a|0;
        return 368
    }
    function Xa()
    {
        var a=0;
        a=c[16]|0;
        c[16]=a+0;
        return a|0
    }
    function Ya(a)
    {
        a=a|0;
        return
    }
    function Za(a)
    {
        a=a|0;
        return
    }
    function _a(a)
    {
        a=a|0;
        return
    }
    function $a(a)
    {
        a=a|0;
        return
    }
    function ab(a)
    {
        a=a|0;
        return
    }
    function bb(a)
    {
        a=a|0;
        Ta(a);
        return
    }
    function cb(a)
    {
        a=a|0;
        Ta(a);
        return
    }
    function db(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0;
        h=i;
        i=i+64|0;
        g=h;
        if((a|0)!=(b|0))if((b|0)!=0?(f=hb(b,160,216,0)|0,(f|0)!=0):0)
        {
            b=g;
            e=b+56|0;
            do
            {
                c[b>>2]=0;
                b=b+4|0
            }
            while((b|0)<(e|0));
            c[g>>2]=f;
            c[g+8>>2]=a;
            c[g+12>>2]=-1;
            c[g+48>>2]=1;
            Ga[c[(c[f>>2]|0)+28>>2]&3](f,g,c[d>>2]|0,1);
            if((c[g+24>>2]|0)==1)
            {
                c[d>>2]=c[g+16>>2];
                b=1
            }
            else b=0
        }
        else b=0;
        else b=1;
        i=h;
        return b|0
    }
    function eb(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0;
        b=d+16|0;
        g=c[b>>2]|0;
        do if(g)
        {
            if((g|0)!=(e|0))
            {
                f=d+36|0;
                c[f>>2]=(c[f>>2]|0)+1;
                c[d+24>>2]=2;
                a[d+54>>0]=1;
                break
            }
            b=d+24|0;
            if((c[b>>2]|0)==2)c[b>>2]=f
        }
        else
        {
            c[b>>2]=e;
            c[d+24>>2]=f;
            c[d+36>>2]=1
        }
        while(0);
        return
    }
    function fb(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        if((a|0)==(c[b+8>>2]|0))eb(0,b,d,e);
        return
    }
    function gb(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        if((a|0)==(c[b+8>>2]|0))eb(0,b,d,e);
        else
        {
            a=c[a+8>>2]|0;
            Ga[c[(c[a>>2]|0)+28>>2]&3](a,b,d,e)
        }
        return
    }
    function hb(d,e,f,g)
    {
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;
        r=i;
        i=i+64|0;
        q=r;
        p=c[d>>2]|0;
        o=d+(c[p+-8>>2]|0)|0;
        p=c[p+-4>>2]|0;
        c[q>>2]=f;
        c[q+4>>2]=d;
        c[q+8>>2]=e;
        c[q+12>>2]=g;
        h=q+16|0;
        j=q+20|0;
        k=q+24|0;
        l=q+28|0;
        m=q+32|0;
        n=q+40|0;
        g=(p|0)==(f|0);
        d=h;
        e=d+36|0;
        do
        {
            c[d>>2]=0;
            d=d+4|0
        }
        while((d|0)<(e|0));
        b[h+36>>1]=0;
        a[h+38>>0]=0;
        do if(g)
        {
            c[q+48>>2]=1;
            Fa[c[(c[f>>2]|0)+20>>2]&3](f,q,o,o,1,0);
            g=(c[k>>2]|0)==1?o:0
        }
        else
        {
            Ba[c[(c[p>>2]|0)+24>>2]&3](p,q,o,1,0);
            g=c[q+36>>2]|0;
            if(!g)
            {
                g=(c[n>>2]|0)==1&(c[l>>2]|0)==1&(c[m>>2]|0)==1?c[j>>2]|0:0;
                break
            }
            else if((g|0)!=1)
            {
                g=0;
                break
            }
            if((c[k>>2]|0)!=1?!((c[n>>2]|0)==0&(c[l>>2]|0)==1&(c[m>>2]|0)==1):0)
            {
                g=0;
                break
            }
            g=c[h>>2]|0
        }
        while(0);
        i=r;
        return g|0
    }
    function ib(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        a[d+53>>0]=1;
        do if((c[d+4>>2]|0)==(f|0))
        {
            a[d+52>>0]=1;
            f=d+16|0;
            b=c[f>>2]|0;
            if(!b)
            {
                c[f>>2]=e;
                c[d+24>>2]=g;
                c[d+36>>2]=1;
                if(!((g|0)==1?(c[d+48>>2]|0)==1:0))break;
                a[d+54>>0]=1;
                break
            }
            if((b|0)!=(e|0))
            {
                e=d+36|0;
                c[e>>2]=(c[e>>2]|0)+1;
                a[d+54>>0]=1;
                break
            }
            b=d+24|0;
            f=c[b>>2]|0;
            if((f|0)==2)
            {
                c[b>>2]=g;
                f=g
            }
            if((f|0)==1?(c[d+48>>2]|0)==1:0)a[d+54>>0]=1
        }
        while(0);
        return
    }
    function jb(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,i=0,j=0,k=0;
        a:do if((b|0)==(c[d+8>>2]|0))
        {
            if((c[d+4>>2]|0)==(e|0)?(h=d+28|0,(c[h>>2]|0)!=1):0)c[h>>2]=f
        }
        else
        {
            if((b|0)!=(c[d>>2]|0))
            {
                i=c[b+8>>2]|0;
                Ba[c[(c[i>>2]|0)+24>>2]&3](i,d,e,f,g);
                break
            }
            if((c[d+16>>2]|0)!=(e|0)?(i=d+20|0,(c[i>>2]|0)!=(e|0)):0)
            {
                c[d+32>>2]=f;
                f=d+44|0;
                if((c[f>>2]|0)==4)break;
                h=d+52|0;
                a[h>>0]=0;
                k=d+53|0;
                a[k>>0]=0;
                b=c[b+8>>2]|0;
                Fa[c[(c[b>>2]|0)+20>>2]&3](b,d,e,e,1,g);
                if(a[k>>0]|0)
                {
                    if(!(a[h>>0]|0))
                    {
                        h=1;
                        j=13
                    }

                }
                else
                {
                    h=0;
                    j=13
                }
                do if((j|0)==13)
                {
                    c[i>>2]=e;
                    i=d+40|0;
                    c[i>>2]=(c[i>>2]|0)+1;
                    if((c[d+36>>2]|0)==1?(c[d+24>>2]|0)==2:0)
                    {
                        a[d+54>>0]=1;
                        if(h)break
                    }
                    else j=16;
                    if((j|0)==16?h:0)break;
                    c[f>>2]=4;
                    break a
                }
                while(0);
                c[f>>2]=3;
                break
            }
            if((f|0)==1)c[d+32>>2]=1
        }
        while(0);
        return
    }
    function kb(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,i=0;
        do if((b|0)==(c[d+8>>2]|0))
        {
            if((c[d+4>>2]|0)==(e|0)?(i=d+28|0,(c[i>>2]|0)!=1):0)c[i>>2]=f
        }
        else if((b|0)==(c[d>>2]|0))
        {
            if((c[d+16>>2]|0)!=(e|0)?(h=d+20|0,(c[h>>2]|0)!=(e|0)):0)
            {
                c[d+32>>2]=f;
                c[h>>2]=e;
                g=d+40|0;
                c[g>>2]=(c[g>>2]|0)+1;
                if((c[d+36>>2]|0)==1?(c[d+24>>2]|0)==2:0)a[d+54>>0]=1;
                c[d+44>>2]=4;
                break
            }
            if((f|0)==1)c[d+32>>2]=1
        }
        while(0);
        return
    }
    function lb(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        if((a|0)==(c[b+8>>2]|0))ib(0,b,d,e,f);
        else
        {
            a=c[a+8>>2]|0;
            Fa[c[(c[a>>2]|0)+20>>2]&3](a,b,d,e,f,g)
        }
        return
    }
    function mb(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        if((a|0)==(c[b+8>>2]|0))ib(0,b,d,e,f);
        return
    }
    function nb()
    {
        var a=0,b=0,d=0;
        b=384;
        b=Db(c[b>>2]|0,c[b+4>>2]|0,1284865837,1481765933)|0;
        b=rb(b|0,C|0,1,0)|0;
        a=C;
        d=384;
        c[d>>2]=b;
        c[d+4>>2]=a;
        a=ub(b|0,a|0,33)|0;
        return a|0
    }
    function ob(a)
    {
        a=a|0;
        var b=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0;
        do if(a>>>0<245)
        {
            o=a>>>0<11?16:a+11&-8;
            a=o>>>3;
            k=c[98]|0;
            f=k>>>a;
            if(f&3)
            {
                d=(f&1^1)+a|0;
                f=d<<1;
                e=432+(f<<2)|0;
                f=432+(f+2<<2)|0;
                g=c[f>>2]|0;
                h=g+8|0;
                i=c[h>>2]|0;
                do if((e|0)!=(i|0))
                {
                    if(i>>>0<(c[102]|0)>>>0)na();
                    b=i+12|0;
                    if((c[b>>2]|0)==(g|0))
                    {
                        c[b>>2]=e;
                        c[f>>2]=i;
                        break
                    }
                    else na()
                }
                else c[98]=k&~(1<<d);
                while(0);
                L=d<<3;
                c[g+4>>2]=L|3;
                L=g+(L|4)|0;
                c[L>>2]=c[L>>2]|1;
                L=h;
                return L|0
            }
            i=c[100]|0;
            if(o>>>0>i>>>0)
            {
                if(f)
                {
                    e=2<<a;
                    e=f<<a&(e|0-e);
                    e=(e&0-e)+-1|0;
                    j=e>>>12&16;
                    e=e>>>j;
                    h=e>>>5&8;
                    e=e>>>h;
                    g=e>>>2&4;
                    e=e>>>g;
                    d=e>>>1&2;
                    e=e>>>d;
                    f=e>>>1&1;
                    f=(h|j|g|d|f)+(e>>>f)|0;
                    e=f<<1;
                    d=432+(e<<2)|0;
                    e=432+(e+2<<2)|0;
                    g=c[e>>2]|0;
                    j=g+8|0;
                    h=c[j>>2]|0;
                    do if((d|0)!=(h|0))
                    {
                        if(h>>>0<(c[102]|0)>>>0)na();
                        b=h+12|0;
                        if((c[b>>2]|0)==(g|0))
                        {
                            c[b>>2]=d;
                            c[e>>2]=h;
                            l=c[100]|0;
                            break
                        }
                        else na()
                    }
                    else
                    {
                        c[98]=k&~(1<<f);
                        l=i
                    }
                    while(0);
                    L=f<<3;
                    i=L-o|0;
                    c[g+4>>2]=o|3;
                    a=g+o|0;
                    c[g+(o|4)>>2]=i|1;
                    c[g+L>>2]=i;
                    if(l)
                    {
                        h=c[103]|0;
                        d=l>>>3;
                        b=d<<1;
                        e=432+(b<<2)|0;
                        f=c[98]|0;
                        d=1<<d;
                        if(f&d)
                        {
                            f=432+(b+2<<2)|0;
                            b=c[f>>2]|0;
                            if(b>>>0<(c[102]|0)>>>0)na();
                            else
                            {
                                m=f;
                                n=b
                            }

                        }
                        else
                        {
                            c[98]=f|d;
                            m=432+(b+2<<2)|0;
                            n=e
                        }
                        c[m>>2]=h;
                        c[n+12>>2]=h;
                        c[h+8>>2]=n;
                        c[h+12>>2]=e
                    }
                    c[100]=i;
                    c[103]=a;
                    L=j;
                    return L|0
                }
                a=c[99]|0;
                if(a)
                {
                    d=(a&0-a)+-1|0;
                    K=d>>>12&16;
                    d=d>>>K;
                    H=d>>>5&8;
                    d=d>>>H;
                    L=d>>>2&4;
                    d=d>>>L;
                    f=d>>>1&2;
                    d=d>>>f;
                    e=d>>>1&1;
                    e=c[696+((H|K|L|f|e)+(d>>>e)<<2)>>2]|0;
                    d=(c[e+4>>2]&-8)-o|0;
                    f=e;
                    while(1)
                    {
                        b=c[f+16>>2]|0;
                        if(!b)
                        {
                            b=c[f+20>>2]|0;
                            if(!b)
                            {
                                k=d;
                                break
                            }

                        }
                        f=(c[b+4>>2]&-8)-o|0;
                        L=f>>>0<d>>>0;
                        d=L?f:d;
                        f=b;
                        e=L?b:e
                    }
                    a=c[102]|0;
                    if(e>>>0<a>>>0)na();
                    i=e+o|0;
                    if(e>>>0>=i>>>0)na();
                    j=c[e+24>>2]|0;
                    d=c[e+12>>2]|0;
                    do if((d|0)==(e|0))
                    {
                        f=e+20|0;
                        b=c[f>>2]|0;
                        if(!b)
                        {
                            f=e+16|0;
                            b=c[f>>2]|0;
                            if(!b)
                            {
                                g=0;
                                break
                            }

                        }
                        while(1)
                        {
                            d=b+20|0;
                            h=c[d>>2]|0;
                            if(h)
                            {
                                b=h;
                                f=d;
                                continue
                            }
                            d=b+16|0;
                            h=c[d>>2]|0;
                            if(!h)break;
                            else
                            {
                                b=h;
                                f=d
                            }

                        }
                        if(f>>>0<a>>>0)na();
                        else
                        {
                            c[f>>2]=0;
                            g=b;
                            break
                        }

                    }
                    else
                    {
                        h=c[e+8>>2]|0;
                        if(h>>>0<a>>>0)na();
                        b=h+12|0;
                        if((c[b>>2]|0)!=(e|0))na();
                        f=d+8|0;
                        if((c[f>>2]|0)==(e|0))
                        {
                            c[b>>2]=d;
                            c[f>>2]=h;
                            g=d;
                            break
                        }
                        else na()
                    }
                    while(0);
                    do if(j)
                    {
                        b=c[e+28>>2]|0;
                        f=696+(b<<2)|0;
                        if((e|0)==(c[f>>2]|0))
                        {
                            c[f>>2]=g;
                            if(!g)
                            {
                                c[99]=c[99]&~(1<<b);
                                break
                            }

                        }
                        else
                        {
                            if(j>>>0<(c[102]|0)>>>0)na();
                            b=j+16|0;
                            if((c[b>>2]|0)==(e|0))c[b>>2]=g;
                            else c[j+20>>2]=g;
                            if(!g)break
                        }
                        f=c[102]|0;
                        if(g>>>0<f>>>0)na();
                        c[g+24>>2]=j;
                        b=c[e+16>>2]|0;
                        do if(b)if(b>>>0<f>>>0)na();
                        else
                        {
                            c[g+16>>2]=b;
                            c[b+24>>2]=g;
                            break
                        }
                        while(0);
                        b=c[e+20>>2]|0;
                        if(b)if(b>>>0<(c[102]|0)>>>0)na();
                        else
                        {
                            c[g+20>>2]=b;
                            c[b+24>>2]=g;
                            break
                        }

                    }
                    while(0);
                    if(k>>>0<16)
                    {
                        L=k+o|0;
                        c[e+4>>2]=L|3;
                        L=e+(L+4)|0;
                        c[L>>2]=c[L>>2]|1
                    }
                    else
                    {
                        c[e+4>>2]=o|3;
                        c[e+(o|4)>>2]=k|1;
                        c[e+(k+o)>>2]=k;
                        b=c[100]|0;
                        if(b)
                        {
                            g=c[103]|0;
                            d=b>>>3;
                            b=d<<1;
                            h=432+(b<<2)|0;
                            f=c[98]|0;
                            d=1<<d;
                            if(f&d)
                            {
                                b=432+(b+2<<2)|0;
                                f=c[b>>2]|0;
                                if(f>>>0<(c[102]|0)>>>0)na();
                                else
                                {
                                    p=b;
                                    q=f
                                }

                            }
                            else
                            {
                                c[98]=f|d;
                                p=432+(b+2<<2)|0;
                                q=h
                            }
                            c[p>>2]=g;
                            c[q+12>>2]=g;
                            c[g+8>>2]=q;
                            c[g+12>>2]=h
                        }
                        c[100]=k;
                        c[103]=i
                    }
                    L=e+8|0;
                    return L|0
                }
                else q=o
            }
            else q=o
        }
        else if(a>>>0<=4294967231)
        {
            a=a+11|0;
            m=a&-8;
            l=c[99]|0;
            if(l)
            {
                f=0-m|0;
                a=a>>>8;
                if(a)if(m>>>0>16777215)k=31;
                else
                {
                    q=(a+1048320|0)>>>16&8;
                    v=a<<q;
                    p=(v+520192|0)>>>16&4;
                    v=v<<p;
                    k=(v+245760|0)>>>16&2;
                    k=14-(p|q|k)+(v<<k>>>15)|0;
                    k=m>>>(k+7|0)&1|k<<1
                }
                else k=0;
                a=c[696+(k<<2)>>2]|0;
                a:do if(!a)
                {
                    d=0;
                    a=0;
                    v=86
                }
                else
                {
                    h=f;
                    d=0;
                    g=m<<((k|0)==31?0:25-(k>>>1)|0);
                    i=a;
                    a=0;
                    while(1)
                    {
                        j=c[i+4>>2]&-8;
                        f=j-m|0;
                        if(f>>>0<h>>>0)if((j|0)==(m|0))
                        {
                            j=i;
                            a=i;
                            v=90;
                            break a
                        }
                        else a=i;
                        else f=h;
                        v=c[i+20>>2]|0;
                        i=c[i+16+(g>>>31<<2)>>2]|0;
                        d=(v|0)==0|(v|0)==(i|0)?d:v;
                        if(!i)
                        {
                            v=86;
                            break
                        }
                        else
                        {
                            h=f;
                            g=g<<1
                        }

                    }

                }
                while(0);
                if((v|0)==86)
                {
                    if((d|0)==0&(a|0)==0)
                    {
                        a=2<<k;
                        a=l&(a|0-a);
                        if(!a)
                        {
                            q=m;
                            break
                        }
                        a=(a&0-a)+-1|0;
                        n=a>>>12&16;
                        a=a>>>n;
                        l=a>>>5&8;
                        a=a>>>l;
                        p=a>>>2&4;
                        a=a>>>p;
                        q=a>>>1&2;
                        a=a>>>q;
                        d=a>>>1&1;
                        d=c[696+((l|n|p|q|d)+(a>>>d)<<2)>>2]|0;
                        a=0
                    }
                    if(!d)
                    {
                        g=f;
                        i=a
                    }
                    else
                    {
                        j=d;
                        v=90
                    }

                }
                if((v|0)==90)while(1)
                    {
                    v=0;
                    q=(c[j+4>>2]&-8)-m|0;
                    d=q>>>0<f>>>0;
                    f=d?q:f;
                    a=d?j:a;
                    d=c[j+16>>2]|0;
                    if(d)
                    {
                        j=d;
                        v=90;
                        continue
                    }
                    j=c[j+20>>2]|0;
                    if(!j)
                    {
                        g=f;
                        i=a;
                        break
                    }
                    else v=90
                }
                if((i|0)!=0?g>>>0<((c[100]|0)-m|0)>>>0:0)
                {
                    a=c[102]|0;
                    if(i>>>0<a>>>0)na();
                    h=i+m|0;
                    if(i>>>0>=h>>>0)na();
                    j=c[i+24>>2]|0;
                    d=c[i+12>>2]|0;
                    do if((d|0)==(i|0))
                    {
                        f=i+20|0;
                        b=c[f>>2]|0;
                        if(!b)
                        {
                            f=i+16|0;
                            b=c[f>>2]|0;
                            if(!b)
                            {
                                o=0;
                                break
                            }

                        }
                        while(1)
                        {
                            d=b+20|0;
                            e=c[d>>2]|0;
                            if(e)
                            {
                                b=e;
                                f=d;
                                continue
                            }
                            d=b+16|0;
                            e=c[d>>2]|0;
                            if(!e)break;
                            else
                            {
                                b=e;
                                f=d
                            }

                        }
                        if(f>>>0<a>>>0)na();
                        else
                        {
                            c[f>>2]=0;
                            o=b;
                            break
                        }

                    }
                    else
                    {
                        e=c[i+8>>2]|0;
                        if(e>>>0<a>>>0)na();
                        b=e+12|0;
                        if((c[b>>2]|0)!=(i|0))na();
                        f=d+8|0;
                        if((c[f>>2]|0)==(i|0))
                        {
                            c[b>>2]=d;
                            c[f>>2]=e;
                            o=d;
                            break
                        }
                        else na()
                    }
                    while(0);
                    do if(j)
                    {
                        b=c[i+28>>2]|0;
                        f=696+(b<<2)|0;
                        if((i|0)==(c[f>>2]|0))
                        {
                            c[f>>2]=o;
                            if(!o)
                            {
                                c[99]=c[99]&~(1<<b);
                                break
                            }

                        }
                        else
                        {
                            if(j>>>0<(c[102]|0)>>>0)na();
                            b=j+16|0;
                            if((c[b>>2]|0)==(i|0))c[b>>2]=o;
                            else c[j+20>>2]=o;
                            if(!o)break
                        }
                        f=c[102]|0;
                        if(o>>>0<f>>>0)na();
                        c[o+24>>2]=j;
                        b=c[i+16>>2]|0;
                        do if(b)if(b>>>0<f>>>0)na();
                        else
                        {
                            c[o+16>>2]=b;
                            c[b+24>>2]=o;
                            break
                        }
                        while(0);
                        b=c[i+20>>2]|0;
                        if(b)if(b>>>0<(c[102]|0)>>>0)na();
                        else
                        {
                            c[o+20>>2]=b;
                            c[b+24>>2]=o;
                            break
                        }

                    }
                    while(0);
                    b:do if(g>>>0>=16)
                    {
                        c[i+4>>2]=m|3;
                        c[i+(m|4)>>2]=g|1;
                        c[i+(g+m)>>2]=g;
                        b=g>>>3;
                        if(g>>>0<256)
                        {
                            f=b<<1;
                            e=432+(f<<2)|0;
                            d=c[98]|0;
                            b=1<<b;
                            if(d&b)
                            {
                                b=432+(f+2<<2)|0;
                                f=c[b>>2]|0;
                                if(f>>>0<(c[102]|0)>>>0)na();
                                else
                                {
                                    s=b;
                                    t=f
                                }

                            }
                            else
                            {
                                c[98]=d|b;
                                s=432+(f+2<<2)|0;
                                t=e
                            }
                            c[s>>2]=h;
                            c[t+12>>2]=h;
                            c[i+(m+8)>>2]=t;
                            c[i+(m+12)>>2]=e;
                            break
                        }
                        b=g>>>8;
                        if(b)if(g>>>0>16777215)e=31;
                        else
                        {
                            K=(b+1048320|0)>>>16&8;
                            L=b<<K;
                            H=(L+520192|0)>>>16&4;
                            L=L<<H;
                            e=(L+245760|0)>>>16&2;
                            e=14-(H|K|e)+(L<<e>>>15)|0;
                            e=g>>>(e+7|0)&1|e<<1
                        }
                        else e=0;
                        b=696+(e<<2)|0;
                        c[i+(m+28)>>2]=e;
                        c[i+(m+20)>>2]=0;
                        c[i+(m+16)>>2]=0;
                        f=c[99]|0;
                        d=1<<e;
                        if(!(f&d))
                        {
                            c[99]=f|d;
                            c[b>>2]=h;
                            c[i+(m+24)>>2]=b;
                            c[i+(m+12)>>2]=h;
                            c[i+(m+8)>>2]=h;
                            break
                        }
                        b=c[b>>2]|0;
                        c:do if((c[b+4>>2]&-8|0)!=(g|0))
                        {
                            e=g<<((e|0)==31?0:25-(e>>>1)|0);
                            while(1)
                            {
                                d=b+16+(e>>>31<<2)|0;
                                f=c[d>>2]|0;
                                if(!f)break;
                                if((c[f+4>>2]&-8|0)==(g|0))
                                {
                                    y=f;
                                    break c
                                }
                                else
                                {
                                    e=e<<1;
                                    b=f
                                }

                            }
                            if(d>>>0<(c[102]|0)>>>0)na();
                            else
                            {
                                c[d>>2]=h;
                                c[i+(m+24)>>2]=b;
                                c[i+(m+12)>>2]=h;
                                c[i+(m+8)>>2]=h;
                                break b
                            }

                        }
                        else y=b;
                        while(0);
                        b=y+8|0;
                        d=c[b>>2]|0;
                        L=c[102]|0;
                        if(d>>>0>=L>>>0&y>>>0>=L>>>0)
                        {
                            c[d+12>>2]=h;
                            c[b>>2]=h;
                            c[i+(m+8)>>2]=d;
                            c[i+(m+12)>>2]=y;
                            c[i+(m+24)>>2]=0;
                            break
                        }
                        else na()
                    }
                    else
                    {
                        L=g+m|0;
                        c[i+4>>2]=L|3;
                        L=i+(L+4)|0;
                        c[L>>2]=c[L>>2]|1
                    }
                    while(0);
                    L=i+8|0;
                    return L|0
                }
                else q=m
            }
            else q=m
        }
        else q=-1;
        while(0);
        a=c[100]|0;
        if(a>>>0>=q>>>0)
        {
            b=a-q|0;
            d=c[103]|0;
            if(b>>>0>15)
            {
                c[103]=d+q;
                c[100]=b;
                c[d+(q+4)>>2]=b|1;
                c[d+a>>2]=b;
                c[d+4>>2]=q|3
            }
            else
            {
                c[100]=0;
                c[103]=0;
                c[d+4>>2]=a|3;
                L=d+(a+4)|0;
                c[L>>2]=c[L>>2]|1
            }
            L=d+8|0;
            return L|0
        }
        a=c[101]|0;
        if(a>>>0>q>>>0)
        {
            K=a-q|0;
            c[101]=K;
            L=c[104]|0;
            c[104]=L+q;
            c[L+(q+4)>>2]=K|1;
            c[L+4>>2]=q|3;
            L=L+8|0;
            return L|0
        }
        do if(!(c[216]|0))
        {
            a=wa(30)|0;
            if(!(a+-1&a))
            {
                c[218]=a;
                c[217]=a;
                c[219]=-1;
                c[220]=-1;
                c[221]=0;
                c[209]=0;
                c[216]=(qa(0)|0)&-16^1431655768;
                break
            }
            else na()
        }
        while(0);
        i=q+48|0;
        g=c[218]|0;
        k=q+47|0;
        h=g+k|0;
        g=0-g|0;
        l=h&g;
        if(l>>>0<=q>>>0)
        {
            L=0;
            return L|0
        }
        a=c[208]|0;
        if((a|0)!=0?(t=c[206]|0,y=t+l|0,y>>>0<=t>>>0|y>>>0>a>>>0):0)
        {
            L=0;
            return L|0
        }
        d:do if(!(c[209]&4))
        {
            a=c[104]|0;
            e:do if(a)
            {
                d=840;
                while(1)
                {
                    f=c[d>>2]|0;
                    if(f>>>0<=a>>>0?(r=d+4|0,(f+(c[r>>2]|0)|0)>>>0>a>>>0):0)
                    {
                        j=d;
                        a=r;
                        break
                    }
                    d=c[d+8>>2]|0;
                    if(!d)
                    {
                        v=174;
                        break e
                    }

                }
                f=h-(c[101]|0)&g;
                if(f>>>0<2147483647)
                {
                    d=pa(f|0)|0;
                    y=(d|0)==((c[j>>2]|0)+(c[a>>2]|0)|0);
                    a=y?f:0;
                    if(y)
                    {
                        if((d|0)!=(-1|0))
                        {
                            w=d;
                            p=a;
                            v=194;
                            break d
                        }

                    }
                    else v=184
                }
                else a=0
            }
            else v=174;
            while(0);
            do if((v|0)==174)
            {
                j=pa(0)|0;
                if((j|0)!=(-1|0))
                {
                    a=j;
                    f=c[217]|0;
                    d=f+-1|0;
                    if(!(d&a))f=l;
                    else f=l-a+(d+a&0-f)|0;
                    a=c[206]|0;
                    d=a+f|0;
                    if(f>>>0>q>>>0&f>>>0<2147483647)
                    {
                        y=c[208]|0;
                        if((y|0)!=0?d>>>0<=a>>>0|d>>>0>y>>>0:0)
                        {
                            a=0;
                            break
                        }
                        d=pa(f|0)|0;
                        y=(d|0)==(j|0);
                        a=y?f:0;
                        if(y)
                        {
                            w=j;
                            p=a;
                            v=194;
                            break d
                        }
                        else v=184
                    }
                    else a=0
                }
                else a=0
            }
            while(0);
            f:do if((v|0)==184)
            {
                j=0-f|0;
                do if(i>>>0>f>>>0&(f>>>0<2147483647&(d|0)!=(-1|0))?(u=c[218]|0,u=k-f+u&0-u,u>>>0<2147483647):0)if((pa(u|0)|0)==(-1|0))
                {
                    pa(j|0)|0;
                    break f
                }
                else
                {
                    f=u+f|0;
                    break
                }
                while(0);
                if((d|0)!=(-1|0))
                {
                    w=d;
                    p=f;
                    v=194;
                    break d
                }

            }
            while(0);
            c[209]=c[209]|4;
            v=191
        }
        else
        {
            a=0;
            v=191
        }
        while(0);
        if((((v|0)==191?l>>>0<2147483647:0)?(w=pa(l|0)|0,x=pa(0)|0,w>>>0<x>>>0&((w|0)!=(-1|0)&(x|0)!=(-1|0))):0)?(z=x-w|0,A=z>>>0>(q+40|0)>>>0,A):0)
        {
            p=A?z:a;
            v=194
        }
        if((v|0)==194)
        {
            a=(c[206]|0)+p|0;
            c[206]=a;
            if(a>>>0>(c[207]|0)>>>0)c[207]=a;
            g=c[104]|0;
            g:do if(g)
            {
                j=840;
                do
                {
                    a=c[j>>2]|0;
                    f=j+4|0;
                    d=c[f>>2]|0;
                    if((w|0)==(a+d|0))
                    {
                        B=a;
                        C=f;
                        D=d;
                        E=j;
                        v=204;
                        break
                    }
                    j=c[j+8>>2]|0
                }
                while((j|0)!=0);
                if(((v|0)==204?(c[E+12>>2]&8|0)==0:0)?g>>>0<w>>>0&g>>>0>=B>>>0:0)
                {
                    c[C>>2]=D+p;
                    L=(c[101]|0)+p|0;
                    K=g+8|0;
                    K=(K&7|0)==0?0:0-K&7;
                    H=L-K|0;
                    c[104]=g+K;
                    c[101]=H;
                    c[g+(K+4)>>2]=H|1;
                    c[g+(L+4)>>2]=40;
                    c[105]=c[220];
                    break
                }
                a=c[102]|0;
                if(w>>>0<a>>>0)
                {
                    c[102]=w;
                    a=w
                }
                f=w+p|0;
                h=840;
                while(1)
                {
                    if((c[h>>2]|0)==(f|0))
                    {
                        d=h;
                        f=h;
                        v=212;
                        break
                    }
                    h=c[h+8>>2]|0;
                    if(!h)
                    {
                        d=840;
                        break
                    }

                }
                if((v|0)==212)if(!(c[f+12>>2]&8))
                {
                    c[d>>2]=w;
                    n=f+4|0;
                    c[n>>2]=(c[n>>2]|0)+p;
                    n=w+8|0;
                    n=(n&7|0)==0?0:0-n&7;
                    k=w+(p+8)|0;
                    k=(k&7|0)==0?0:0-k&7;
                    b=w+(k+p)|0;
                    m=n+q|0;
                    o=w+m|0;
                    l=b-(w+n)-q|0;
                    c[w+(n+4)>>2]=q|3;
                    h:do if((b|0)!=(g|0))
                    {
                        if((b|0)==(c[103]|0))
                        {
                            L=(c[100]|0)+l|0;
                            c[100]=L;
                            c[103]=o;
                            c[w+(m+4)>>2]=L|1;
                            c[w+(L+m)>>2]=L;
                            break
                        }
                        g=p+4|0;
                        f=c[w+(g+k)>>2]|0;
                        if((f&3|0)==1)
                        {
                            i=f&-8;
                            h=f>>>3;
                            i:do if(f>>>0>=256)
                            {
                                j=c[w+((k|24)+p)>>2]|0;
                                d=c[w+(p+12+k)>>2]|0;
                                do if((d|0)==(b|0))
                                {
                                    e=k|16;
                                    d=w+(g+e)|0;
                                    f=c[d>>2]|0;
                                    if(!f)
                                    {
                                        d=w+(e+p)|0;
                                        f=c[d>>2]|0;
                                        if(!f)
                                        {
                                            J=0;
                                            break
                                        }

                                    }
                                    while(1)
                                    {
                                        e=f+20|0;
                                        h=c[e>>2]|0;
                                        if(h)
                                        {
                                            f=h;
                                            d=e;
                                            continue
                                        }
                                        e=f+16|0;
                                        h=c[e>>2]|0;
                                        if(!h)break;
                                        else
                                        {
                                            f=h;
                                            d=e
                                        }

                                    }
                                    if(d>>>0<a>>>0)na();
                                    else
                                    {
                                        c[d>>2]=0;
                                        J=f;
                                        break
                                    }

                                }
                                else
                                {
                                    e=c[w+((k|8)+p)>>2]|0;
                                    if(e>>>0<a>>>0)na();
                                    a=e+12|0;
                                    if((c[a>>2]|0)!=(b|0))na();
                                    f=d+8|0;
                                    if((c[f>>2]|0)==(b|0))
                                    {
                                        c[a>>2]=d;
                                        c[f>>2]=e;
                                        J=d;
                                        break
                                    }
                                    else na()
                                }
                                while(0);
                                if(!j)break;
                                a=c[w+(p+28+k)>>2]|0;
                                f=696+(a<<2)|0;
                                do if((b|0)!=(c[f>>2]|0))
                                {
                                    if(j>>>0<(c[102]|0)>>>0)na();
                                    a=j+16|0;
                                    if((c[a>>2]|0)==(b|0))c[a>>2]=J;
                                    else c[j+20>>2]=J;
                                    if(!J)break i
                                }
                                else
                                {
                                    c[f>>2]=J;
                                    if(J)break;
                                    c[99]=c[99]&~(1<<a);
                                    break i
                                }
                                while(0);
                                f=c[102]|0;
                                if(J>>>0<f>>>0)na();
                                c[J+24>>2]=j;
                                b=k|16;
                                a=c[w+(b+p)>>2]|0;
                                do if(a)if(a>>>0<f>>>0)na();
                                else
                                {
                                    c[J+16>>2]=a;
                                    c[a+24>>2]=J;
                                    break
                                }
                                while(0);
                                b=c[w+(g+b)>>2]|0;
                                if(!b)break;
                                if(b>>>0<(c[102]|0)>>>0)na();
                                else
                                {
                                    c[J+20>>2]=b;
                                    c[b+24>>2]=J;
                                    break
                                }

                            }
                            else
                            {
                                d=c[w+((k|8)+p)>>2]|0;
                                e=c[w+(p+12+k)>>2]|0;
                                f=432+(h<<1<<2)|0;
                                do if((d|0)!=(f|0))
                                {
                                    if(d>>>0<a>>>0)na();
                                    if((c[d+12>>2]|0)==(b|0))break;
                                    na()
                                }
                                while(0);
                                if((e|0)==(d|0))
                                {
                                    c[98]=c[98]&~(1<<h);
                                    break
                                }
                                do if((e|0)==(f|0))F=e+8|0;
                                else
                                {
                                    if(e>>>0<a>>>0)na();
                                    a=e+8|0;
                                    if((c[a>>2]|0)==(b|0))
                                    {
                                        F=a;
                                        break
                                    }
                                    na()
                                }
                                while(0);
                                c[d+12>>2]=e;
                                c[F>>2]=d
                            }
                            while(0);
                            b=w+((i|k)+p)|0;
                            a=i+l|0
                        }
                        else a=l;
                        b=b+4|0;
                        c[b>>2]=c[b>>2]&-2;
                        c[w+(m+4)>>2]=a|1;
                        c[w+(a+m)>>2]=a;
                        b=a>>>3;
                        if(a>>>0<256)
                        {
                            f=b<<1;
                            e=432+(f<<2)|0;
                            d=c[98]|0;
                            b=1<<b;
                            do if(!(d&b))
                            {
                                c[98]=d|b;
                                K=432+(f+2<<2)|0;
                                L=e
                            }
                            else
                            {
                                b=432+(f+2<<2)|0;
                                f=c[b>>2]|0;
                                if(f>>>0>=(c[102]|0)>>>0)
                                {
                                    K=b;
                                    L=f;
                                    break
                                }
                                na()
                            }
                            while(0);
                            c[K>>2]=o;
                            c[L+12>>2]=o;
                            c[w+(m+8)>>2]=L;
                            c[w+(m+12)>>2]=e;
                            break
                        }
                        b=a>>>8;
                        do if(!b)e=0;
                        else
                        {
                            if(a>>>0>16777215)
                            {
                                e=31;
                                break
                            }
                            K=(b+1048320|0)>>>16&8;
                            L=b<<K;
                            H=(L+520192|0)>>>16&4;
                            L=L<<H;
                            e=(L+245760|0)>>>16&2;
                            e=14-(H|K|e)+(L<<e>>>15)|0;
                            e=a>>>(e+7|0)&1|e<<1
                        }
                        while(0);
                        b=696+(e<<2)|0;
                        c[w+(m+28)>>2]=e;
                        c[w+(m+20)>>2]=0;
                        c[w+(m+16)>>2]=0;
                        f=c[99]|0;
                        d=1<<e;
                        if(!(f&d))
                        {
                            c[99]=f|d;
                            c[b>>2]=o;
                            c[w+(m+24)>>2]=b;
                            c[w+(m+12)>>2]=o;
                            c[w+(m+8)>>2]=o;
                            break
                        }
                        b=c[b>>2]|0;
                        j:do if((c[b+4>>2]&-8|0)!=(a|0))
                        {
                            e=a<<((e|0)==31?0:25-(e>>>1)|0);
                            while(1)
                            {
                                d=b+16+(e>>>31<<2)|0;
                                f=c[d>>2]|0;
                                if(!f)break;
                                if((c[f+4>>2]&-8|0)==(a|0))
                                {
                                    M=f;
                                    break j
                                }
                                else
                                {
                                    e=e<<1;
                                    b=f
                                }

                            }
                            if(d>>>0<(c[102]|0)>>>0)na();
                            else
                            {
                                c[d>>2]=o;
                                c[w+(m+24)>>2]=b;
                                c[w+(m+12)>>2]=o;
                                c[w+(m+8)>>2]=o;
                                break h
                            }

                        }
                        else M=b;
                        while(0);
                        b=M+8|0;
                        d=c[b>>2]|0;
                        L=c[102]|0;
                        if(d>>>0>=L>>>0&M>>>0>=L>>>0)
                        {
                            c[d+12>>2]=o;
                            c[b>>2]=o;
                            c[w+(m+8)>>2]=d;
                            c[w+(m+12)>>2]=M;
                            c[w+(m+24)>>2]=0;
                            break
                        }
                        else na()
                    }
                    else
                    {
                        L=(c[101]|0)+l|0;
                        c[101]=L;
                        c[104]=o;
                        c[w+(m+4)>>2]=L|1
                    }
                    while(0);
                    L=w+(n|8)|0;
                    return L|0
                }
                else d=840;
                while(1)
                {
                    f=c[d>>2]|0;
                    if(f>>>0<=g>>>0?(b=c[d+4>>2]|0,e=f+b|0,e>>>0>g>>>0):0)break;
                    d=c[d+8>>2]|0
                }
                a=f+(b+-39)|0;
                f=f+(b+-47+((a&7|0)==0?0:0-a&7))|0;
                a=g+16|0;
                f=f>>>0<a>>>0?g:f;
                b=f+8|0;
                d=w+8|0;
                d=(d&7|0)==0?0:0-d&7;
                L=p+-40-d|0;
                c[104]=w+d;
                c[101]=L;
                c[w+(d+4)>>2]=L|1;
                c[w+(p+-36)>>2]=40;
                c[105]=c[220];
                d=f+4|0;
                c[d>>2]=27;
                c[b>>2]=c[210];
                c[b+4>>2]=c[211];
                c[b+8>>2]=c[212];
                c[b+12>>2]=c[213];
                c[210]=w;
                c[211]=p;
                c[213]=0;
                c[212]=b;
                b=f+28|0;
                c[b>>2]=7;
                if((f+32|0)>>>0<e>>>0)do
                    {
                    L=b;
                    b=b+4|0;
                    c[b>>2]=7
                }
                while((L+8|0)>>>0<e>>>0);
                if((f|0)!=(g|0))
                {
                    h=f-g|0;
                    c[d>>2]=c[d>>2]&-2;
                    c[g+4>>2]=h|1;
                    c[f>>2]=h;
                    b=h>>>3;
                    if(h>>>0<256)
                    {
                        f=b<<1;
                        e=432+(f<<2)|0;
                        d=c[98]|0;
                        b=1<<b;
                        if(d&b)
                        {
                            b=432+(f+2<<2)|0;
                            d=c[b>>2]|0;
                            if(d>>>0<(c[102]|0)>>>0)na();
                            else
                            {
                                G=b;
                                H=d
                            }

                        }
                        else
                        {
                            c[98]=d|b;
                            G=432+(f+2<<2)|0;
                            H=e
                        }
                        c[G>>2]=g;
                        c[H+12>>2]=g;
                        c[g+8>>2]=H;
                        c[g+12>>2]=e;
                        break
                    }
                    b=h>>>8;
                    if(b)if(h>>>0>16777215)e=31;
                    else
                    {
                        K=(b+1048320|0)>>>16&8;
                        L=b<<K;
                        H=(L+520192|0)>>>16&4;
                        L=L<<H;
                        e=(L+245760|0)>>>16&2;
                        e=14-(H|K|e)+(L<<e>>>15)|0;
                        e=h>>>(e+7|0)&1|e<<1
                    }
                    else e=0;
                    f=696+(e<<2)|0;
                    c[g+28>>2]=e;
                    c[g+20>>2]=0;
                    c[a>>2]=0;
                    b=c[99]|0;
                    d=1<<e;
                    if(!(b&d))
                    {
                        c[99]=b|d;
                        c[f>>2]=g;
                        c[g+24>>2]=f;
                        c[g+12>>2]=g;
                        c[g+8>>2]=g;
                        break
                    }
                    b=c[f>>2]|0;
                    k:do if((c[b+4>>2]&-8|0)!=(h|0))
                    {
                        f=h<<((e|0)==31?0:25-(e>>>1)|0);
                        while(1)
                        {
                            d=b+16+(f>>>31<<2)|0;
                            e=c[d>>2]|0;
                            if(!e)break;
                            if((c[e+4>>2]&-8|0)==(h|0))
                            {
                                I=e;
                                break k
                            }
                            else
                            {
                                f=f<<1;
                                b=e
                            }

                        }
                        if(d>>>0<(c[102]|0)>>>0)na();
                        else
                        {
                            c[d>>2]=g;
                            c[g+24>>2]=b;
                            c[g+12>>2]=g;
                            c[g+8>>2]=g;
                            break g
                        }

                    }
                    else I=b;
                    while(0);
                    b=I+8|0;
                    d=c[b>>2]|0;
                    L=c[102]|0;
                    if(d>>>0>=L>>>0&I>>>0>=L>>>0)
                    {
                        c[d+12>>2]=g;
                        c[b>>2]=g;
                        c[g+8>>2]=d;
                        c[g+12>>2]=I;
                        c[g+24>>2]=0;
                        break
                    }
                    else na()
                }

            }
            else
            {
                L=c[102]|0;
                if((L|0)==0|w>>>0<L>>>0)c[102]=w;
                c[210]=w;
                c[211]=p;
                c[213]=0;
                c[107]=c[216];
                c[106]=-1;
                b=0;
                do
                {
                    L=b<<1;
                    K=432+(L<<2)|0;
                    c[432+(L+3<<2)>>2]=K;
                    c[432+(L+2<<2)>>2]=K;
                    b=b+1|0
                }
                while((b|0)!=32);
                L=w+8|0;
                L=(L&7|0)==0?0:0-L&7;
                K=p+-40-L|0;
                c[104]=w+L;
                c[101]=K;
                c[w+(L+4)>>2]=K|1;
                c[w+(p+-36)>>2]=40;
                c[105]=c[220]
            }
            while(0);
            b=c[101]|0;
            if(b>>>0>q>>>0)
            {
                K=b-q|0;
                c[101]=K;
                L=c[104]|0;
                c[104]=L+q;
                c[L+(q+4)>>2]=K|1;
                c[L+4>>2]=q|3;
                L=L+8|0;
                return L|0
            }

        }
        c[(ta()|0)>>2]=12;
        L=0;
        return L|0
    }
    function pb(a)
    {
        a=a|0;
        var b=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;
        if(!a)return;
        b=a+-8|0;
        i=c[102]|0;
        if(b>>>0<i>>>0)na();
        f=c[a+-4>>2]|0;
        d=f&3;
        if((d|0)==1)na();
        o=f&-8;
        q=a+(o+-8)|0;
        do if(!(f&1))
        {
            b=c[b>>2]|0;
            if(!d)return;
            j=-8-b|0;
            l=a+j|0;
            m=b+o|0;
            if(l>>>0<i>>>0)na();
            if((l|0)==(c[103]|0))
            {
                b=a+(o+-4)|0;
                f=c[b>>2]|0;
                if((f&3|0)!=3)
                {
                    u=l;
                    g=m;
                    break
                }
                c[100]=m;
                c[b>>2]=f&-2;
                c[a+(j+4)>>2]=m|1;
                c[q>>2]=m;
                return
            }
            e=b>>>3;
            if(b>>>0<256)
            {
                d=c[a+(j+8)>>2]|0;
                f=c[a+(j+12)>>2]|0;
                b=432+(e<<1<<2)|0;
                if((d|0)!=(b|0))
                {
                    if(d>>>0<i>>>0)na();
                    if((c[d+12>>2]|0)!=(l|0))na()
                }
                if((f|0)==(d|0))
                {
                    c[98]=c[98]&~(1<<e);
                    u=l;
                    g=m;
                    break
                }
                if((f|0)!=(b|0))
                {
                    if(f>>>0<i>>>0)na();
                    b=f+8|0;
                    if((c[b>>2]|0)==(l|0))h=b;
                    else na()
                }
                else h=f+8|0;
                c[d+12>>2]=f;
                c[h>>2]=d;
                u=l;
                g=m;
                break
            }
            h=c[a+(j+24)>>2]|0;
            d=c[a+(j+12)>>2]|0;
            do if((d|0)==(l|0))
            {
                f=a+(j+20)|0;
                b=c[f>>2]|0;
                if(!b)
                {
                    f=a+(j+16)|0;
                    b=c[f>>2]|0;
                    if(!b)
                    {
                        k=0;
                        break
                    }

                }
                while(1)
                {
                    d=b+20|0;
                    e=c[d>>2]|0;
                    if(e)
                    {
                        b=e;
                        f=d;
                        continue
                    }
                    d=b+16|0;
                    e=c[d>>2]|0;
                    if(!e)break;
                    else
                    {
                        b=e;
                        f=d
                    }

                }
                if(f>>>0<i>>>0)na();
                else
                {
                    c[f>>2]=0;
                    k=b;
                    break
                }

            }
            else
            {
                e=c[a+(j+8)>>2]|0;
                if(e>>>0<i>>>0)na();
                b=e+12|0;
                if((c[b>>2]|0)!=(l|0))na();
                f=d+8|0;
                if((c[f>>2]|0)==(l|0))
                {
                    c[b>>2]=d;
                    c[f>>2]=e;
                    k=d;
                    break
                }
                else na()
            }
            while(0);
            if(h)
            {
                b=c[a+(j+28)>>2]|0;
                f=696+(b<<2)|0;
                if((l|0)==(c[f>>2]|0))
                {
                    c[f>>2]=k;
                    if(!k)
                    {
                        c[99]=c[99]&~(1<<b);
                        u=l;
                        g=m;
                        break
                    }

                }
                else
                {
                    if(h>>>0<(c[102]|0)>>>0)na();
                    b=h+16|0;
                    if((c[b>>2]|0)==(l|0))c[b>>2]=k;
                    else c[h+20>>2]=k;
                    if(!k)
                    {
                        u=l;
                        g=m;
                        break
                    }

                }
                f=c[102]|0;
                if(k>>>0<f>>>0)na();
                c[k+24>>2]=h;
                b=c[a+(j+16)>>2]|0;
                do if(b)if(b>>>0<f>>>0)na();
                else
                {
                    c[k+16>>2]=b;
                    c[b+24>>2]=k;
                    break
                }
                while(0);
                b=c[a+(j+20)>>2]|0;
                if(b)if(b>>>0<(c[102]|0)>>>0)na();
                else
                {
                    c[k+20>>2]=b;
                    c[b+24>>2]=k;
                    u=l;
                    g=m;
                    break
                }
                else
                {
                    u=l;
                    g=m
                }

            }
            else
            {
                u=l;
                g=m
            }

        }
        else
        {
            u=b;
            g=o
        }
        while(0);
        if(u>>>0>=q>>>0)na();
        b=a+(o+-4)|0;
        f=c[b>>2]|0;
        if(!(f&1))na();
        if(!(f&2))
        {
            if((q|0)==(c[104]|0))
            {
                t=(c[101]|0)+g|0;
                c[101]=t;
                c[104]=u;
                c[u+4>>2]=t|1;
                if((u|0)!=(c[103]|0))return;
                c[103]=0;
                c[100]=0;
                return
            }
            if((q|0)==(c[103]|0))
            {
                t=(c[100]|0)+g|0;
                c[100]=t;
                c[103]=u;
                c[u+4>>2]=t|1;
                c[u+t>>2]=t;
                return
            }
            g=(f&-8)+g|0;
            e=f>>>3;
            do if(f>>>0>=256)
            {
                h=c[a+(o+16)>>2]|0;
                b=c[a+(o|4)>>2]|0;
                do if((b|0)==(q|0))
                {
                    f=a+(o+12)|0;
                    b=c[f>>2]|0;
                    if(!b)
                    {
                        f=a+(o+8)|0;
                        b=c[f>>2]|0;
                        if(!b)
                        {
                            p=0;
                            break
                        }

                    }
                    while(1)
                    {
                        d=b+20|0;
                        e=c[d>>2]|0;
                        if(e)
                        {
                            b=e;
                            f=d;
                            continue
                        }
                        d=b+16|0;
                        e=c[d>>2]|0;
                        if(!e)break;
                        else
                        {
                            b=e;
                            f=d
                        }

                    }
                    if(f>>>0<(c[102]|0)>>>0)na();
                    else
                    {
                        c[f>>2]=0;
                        p=b;
                        break
                    }

                }
                else
                {
                    f=c[a+o>>2]|0;
                    if(f>>>0<(c[102]|0)>>>0)na();
                    d=f+12|0;
                    if((c[d>>2]|0)!=(q|0))na();
                    e=b+8|0;
                    if((c[e>>2]|0)==(q|0))
                    {
                        c[d>>2]=b;
                        c[e>>2]=f;
                        p=b;
                        break
                    }
                    else na()
                }
                while(0);
                if(h)
                {
                    b=c[a+(o+20)>>2]|0;
                    f=696+(b<<2)|0;
                    if((q|0)==(c[f>>2]|0))
                    {
                        c[f>>2]=p;
                        if(!p)
                        {
                            c[99]=c[99]&~(1<<b);
                            break
                        }

                    }
                    else
                    {
                        if(h>>>0<(c[102]|0)>>>0)na();
                        b=h+16|0;
                        if((c[b>>2]|0)==(q|0))c[b>>2]=p;
                        else c[h+20>>2]=p;
                        if(!p)break
                    }
                    f=c[102]|0;
                    if(p>>>0<f>>>0)na();
                    c[p+24>>2]=h;
                    b=c[a+(o+8)>>2]|0;
                    do if(b)if(b>>>0<f>>>0)na();
                    else
                    {
                        c[p+16>>2]=b;
                        c[b+24>>2]=p;
                        break
                    }
                    while(0);
                    b=c[a+(o+12)>>2]|0;
                    if(b)if(b>>>0<(c[102]|0)>>>0)na();
                    else
                    {
                        c[p+20>>2]=b;
                        c[b+24>>2]=p;
                        break
                    }

                }

            }
            else
            {
                d=c[a+o>>2]|0;
                f=c[a+(o|4)>>2]|0;
                b=432+(e<<1<<2)|0;
                if((d|0)!=(b|0))
                {
                    if(d>>>0<(c[102]|0)>>>0)na();
                    if((c[d+12>>2]|0)!=(q|0))na()
                }
                if((f|0)==(d|0))
                {
                    c[98]=c[98]&~(1<<e);
                    break
                }
                if((f|0)!=(b|0))
                {
                    if(f>>>0<(c[102]|0)>>>0)na();
                    b=f+8|0;
                    if((c[b>>2]|0)==(q|0))n=b;
                    else na()
                }
                else n=f+8|0;
                c[d+12>>2]=f;
                c[n>>2]=d
            }
            while(0);
            c[u+4>>2]=g|1;
            c[u+g>>2]=g;
            if((u|0)==(c[103]|0))
            {
                c[100]=g;
                return
            }

        }
        else
        {
            c[b>>2]=f&-2;
            c[u+4>>2]=g|1;
            c[u+g>>2]=g
        }
        b=g>>>3;
        if(g>>>0<256)
        {
            d=b<<1;
            f=432+(d<<2)|0;
            e=c[98]|0;
            b=1<<b;
            if(e&b)
            {
                b=432+(d+2<<2)|0;
                d=c[b>>2]|0;
                if(d>>>0<(c[102]|0)>>>0)na();
                else
                {
                    r=b;
                    s=d
                }

            }
            else
            {
                c[98]=e|b;
                r=432+(d+2<<2)|0;
                s=f
            }
            c[r>>2]=u;
            c[s+12>>2]=u;
            c[u+8>>2]=s;
            c[u+12>>2]=f;
            return
        }
        b=g>>>8;
        if(b)if(g>>>0>16777215)f=31;
        else
        {
            r=(b+1048320|0)>>>16&8;
            s=b<<r;
            q=(s+520192|0)>>>16&4;
            s=s<<q;
            f=(s+245760|0)>>>16&2;
            f=14-(q|r|f)+(s<<f>>>15)|0;
            f=g>>>(f+7|0)&1|f<<1
        }
        else f=0;
        b=696+(f<<2)|0;
        c[u+28>>2]=f;
        c[u+20>>2]=0;
        c[u+16>>2]=0;
        d=c[99]|0;
        e=1<<f;
        a:do if(d&e)
        {
            b=c[b>>2]|0;
            b:do if((c[b+4>>2]&-8|0)!=(g|0))
            {
                f=g<<((f|0)==31?0:25-(f>>>1)|0);
                while(1)
                {
                    d=b+16+(f>>>31<<2)|0;
                    e=c[d>>2]|0;
                    if(!e)break;
                    if((c[e+4>>2]&-8|0)==(g|0))
                    {
                        t=e;
                        break b
                    }
                    else
                    {
                        f=f<<1;
                        b=e
                    }

                }
                if(d>>>0<(c[102]|0)>>>0)na();
                else
                {
                    c[d>>2]=u;
                    c[u+24>>2]=b;
                    c[u+12>>2]=u;
                    c[u+8>>2]=u;
                    break a
                }

            }
            else t=b;
            while(0);
            b=t+8|0;
            d=c[b>>2]|0;
            s=c[102]|0;
            if(d>>>0>=s>>>0&t>>>0>=s>>>0)
            {
                c[d+12>>2]=u;
                c[b>>2]=u;
                c[u+8>>2]=d;
                c[u+12>>2]=t;
                c[u+24>>2]=0;
                break
            }
            else na()
        }
        else
        {
            c[99]=d|e;
            c[b>>2]=u;
            c[u+24>>2]=b;
            c[u+12>>2]=u;
            c[u+8>>2]=u
        }
        while(0);
        u=(c[106]|0)+-1|0;
        c[106]=u;
        if(!u)b=848;
        else return;
        while(1)
        {
            b=c[b>>2]|0;
            if(!b)break;
            else b=b+8|0
        }
        c[106]=-1;
        return
    }
    function rb(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        c=a+c>>>0;
        return (C=b+d+(c>>>0<a>>>0|0)>>>0,c|0)|0
    }
    function ub(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        if((c|0)<32)
        {
            C=b>>>c;
            return a>>>c|(b&(1<<c)-1)<<32-c
        }
        C=0;
        return b>>>c-32|0
    }
    function Ab(a,b)
    {
        a=a|0;
        b=b|0;
        var c=0,d=0,e=0,f=0;
        f=a&65535;
        e=b&65535;
        c=_(e,f)|0;
        d=a>>>16;
        a=(c>>>16)+(_(e,d)|0)|0;
        e=b>>>16;
        b=_(e,f)|0;
        return (C=(a>>>16)+(_(e,d)|0)+(((a&65535)+b|0)>>>16)|0,a+b<<16|c&65535|0)|0
    }
    function Db(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        var e=0,f=0;
        e=a;
        f=c;
        c=Ab(e,f)|0;
        a=C;
        return (C=(_(b,f)|0)+(_(d,e)|0)+a|a&0,c|0|0)|0
    }
    function Ob(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        ba(0);
        return 0
    }
    function Pb(a,b,c,d,e)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        ba(1)
    }
    function Qb(a)
    {
        a=a|0;
        ba(2)
    }
    function Rb(a)
    {
        a=a|0;
        ba(3);
        return 0
    }
    function Sb()
    {
        ba(4)
    }
    function Tb(a,b,c,d,e,f)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        ba(5)
    }
    function Ub(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        ba(6)
    }

    // EMSCRIPTEN_END_FUNCS
    var Ba=[Pb,kb,jb,Pb];
    var Ca=[Qb,Ua,Va,_a,bb,$a,ab,cb];
    var Da=[Rb,Wa];
    var Ea=[Sb];
    var Fa=[Tb,mb,lb,Tb];
    var Ga=[Ub,fb,gb,Ub];
    return
    {
        _main:Qa
    }

}
