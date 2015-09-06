(function(global,env,buffer) 
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
    var j=env.STACK_MAX|0;
    var k=env.tempDoublePtr|0;
    var l=env.ABORT|0;
    var m=env.cttz_i8|0;
    var n=env.___dso_handle|0;
    var o=env._stderr|0;
    var p=env._stdin|0;
    var q=env._stdout|0;
    var r=0;
    var s=0;
    var t=0;
    var u=0;
    var v=global.NaN,w=global.Infinity;
    var x=0,y=0,z=0,A=0,B=0.0,C=0,D=0,E=0,F=0.0;
    var G=0;
    var H=0;
    var I=0;
    var J=0;
    var K=0;
    var L=0;
    var M=0;
    var N=0;
    var O=0;
    var P=0;
    var Q=global.Math.floor;
    var R=global.Math.abs;
    var S=global.Math.sqrt;
    var T=global.Math.pow;
    var U=global.Math.cos;
    var V=global.Math.sin;
    var W=global.Math.tan;
    var X=global.Math.acos;
    var Y=global.Math.asin;
    var Z=global.Math.atan;
    var _=global.Math.atan2;
    var $=global.Math.exp;
    var aa=global.Math.log;
    var ba=global.Math.ceil;
    var ca=global.Math.imul;
    var da=global.Math.min;
    var ea=global.Math.clz32;
    var fa=env.abort;
    var ga=env.assert;
    var ha=env.invoke_iiiiiiii;
    var ia=env.invoke_iiii;
    var ja=env.invoke_viiiii;
    var ka=env.invoke_iiiiiid;
    var la=env.invoke_vi;
    var ma=env.invoke_vii;
    var na=env.invoke_iiiiiii;
    var oa=env.invoke_iiiiid;
    var pa=env.invoke_ii;
    var qa=env.invoke_viii;
    var ra=env.invoke_v;
    var sa=env.invoke_iiiiiiiii;
    var ta=env.invoke_iiiii;
    var ua=env.invoke_viiiiii;
    var va=env.invoke_iii;
    var wa=env.invoke_iiiiii;
    var xa=env.invoke_viiii;
    var ya=env._fabs;
    var za=env._strftime;
    var Aa=env._pthread_cond_wait;
    var Ba=env._send;
    var Ca=env._fread;
    var Da=env.___ctype_b_loc;
    var Ea=env.___cxa_guard_acquire;
    var Fa=env.__reallyNegative;
    var Ga=env._vfprintf;
    var Ha=env._ungetc;
    var Ia=env.___assert_fail;
    var Ja=env.___cxa_allocate_exception;
    var Ka=env.___cxa_find_matching_catch;
    var La=env.___ctype_toupper_loc;
    var Ma=env._fflush;
    var Na=env.___cxa_guard_release;
    var Oa=env.__addDays;
    var Pa=env._pwrite;
    var Qa=env._strerror_r;
    var Ra=env._strftime_l;
    var Sa=env._pthread_mutex_lock;
    var Ta=env.___setErrNo;
    var Ua=env._sbrk;
    var Va=env._uselocale;
    var Wa=env._catgets;
    var Xa=env._newlocale;
    var Ya=env.___cxa_begin_catch;
    var Za=env._emscripten_memcpy_big;
    var _a=env._fileno;
    var $a=env.___resumeException;
    var ab=env.__ZSt18uncaught_exceptionv;
    var bb=env._freelocale;
    var cb=env._pthread_getspecific;
    var db=env.__arraySum;
    var eb=env._calloc;
    var fb=env.___ctype_tolower_loc;
    var gb=env._pthread_mutex_unlock;
    var hb=env._pthread_once;
    var ib=env._pread;
    var jb=env._mkport;
    var kb=env._pthread_key_create;
    var lb=env._getc;
    var mb=env._write;
    var nb=env.__isLeapYear;
    var ob=env.___errno_location;
    var pb=env._recv;
    var qb=env._pthread_setspecific;
    var rb=env.___cxa_atexit;
    var sb=env._fgetc;
    var tb=env._fputc;
    var ub=env.___cxa_throw;
    var vb=env._sysconf;
    var wb=env._pthread_cond_broadcast;
    var xb=env._abort;
    var yb=env._catclose;
    var zb=env._fwrite;
    var Ab=env._time;
    var Bb=env._fprintf;
    var Cb=env._strerror;
    var Db=env.__formatString;
    var Eb=env._atexit;
    var Fb=env._catopen;
    var Gb=env._read;
    var Hb=0.0;
    // EMSCRIPTEN_START_FUNCS
    function Ge(b,e)
    {
        b=b|0;
        e=e|0;
        var f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;
        t=i;
        i=i+32|0;
        s=t+16|0;
        r=t+8|0;
        o=t;
        p=t+4|0;
        h=b+52|0;
        a:do if(a[h>>0]|0)
        {
            g=b+48|0;
            f=c[g>>2]|0;
            if(e)
            {
                c[g>>2]=-1;
                a[h>>0]=0
            }
        }
        else
        {
            f=c[b+44>>2]|0;
            f=(f|0)>1?f:1;
            q=b+32|0;
            if((f|0)>0)
            {
                h=0;
                do
                {
                    g=lb(c[q>>2]|0)|0;
                    if((g|0)==-1)
                    {
                        f=-1;
                        break a
                    }
                    a[s+h>>0]=g;
                    h=h+1|0
                }
                while((h|0)<(f|0))
            }
            b:do if(!(a[b+53>>0]|0))
            {
                l=b+40|0;
                m=b+36|0;
                n=r+1|0;
                while(1)
                {
                    j=c[l>>2]|0;
                    h=j;
                    g=c[h>>2]|0;
                    h=c[h+4>>2]|0;
                    u=c[m>>2]|0;
                    k=s+f|0;
                    j=Tb[c[(c[u>>2]|0)+16>>2]&15](u,j,s,k,o,r,n,p)|0;
                    if((j|0)==3)break;
                    else if((j|0)==2)
                    {
                        f=-1;
                        break a
                    }
                    else if((j|0)!=1)break b;
                    u=c[l>>2]|0;
                    c[u>>2]=g;
                    c[u+4>>2]=h;
                    if((f|0)==8)
                    {
                        f=-1;
                        break a
                    }
                    g=lb(c[q>>2]|0)|0;
                    if((g|0)==-1)
                    {
                        f=-1;
                        break a
                    }
                    a[k>>0]=g;
                    f=f+1|0
                }
                a[r>>0]=a[s>>0]|0
            }
            else a[r>>0]=a[s>>0]|0;
            while(0);
            if(e)
            {
                f=a[r>>0]|0;
                c[b+48>>2]=f&255
            }
            else
            {
                while(1)
                {
                    if((f|0)<=0)break;
                    f=f+-1|0;
                    if((Ha(d[s+f>>0]|0,c[q>>2]|0)|0)==-1)
                    {
                        f=-1;
                        break a
                    }
                }
                f=a[r>>0]|0
            }
            f=f&255
        }
        while(0);
        i=t;
        return f|0
    }
    function He(b,d)
    {
        b=b|0;
        d=d|0;
        if(!(a[d>>0]&1))
        {
            c[b>>2]=c[d>>2];
            c[b+4>>2]=c[d+4>>2];
            c[b+8>>2]=c[d+8>>2]
        }
        else Ie(b,c[d+8>>2]|0,c[d+4>>2]|0);
        return
    }
    function Ie(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        if(e>>>0>4294967279)wd(b);
        if(e>>>0<11)
        {
            a[b>>0]=e<<1;
            b=b+1|0
        }
        else
        {
            g=e+16&-16;
            f=Kc(g)|0;
            c[b+8>>2]=f;
            c[b>>2]=g|1;
            c[b+4>>2]=e;
            b=f
        }
        _n(b|0,d|0,e|0)|0;
        a[b+e>>0]=0;
        return
    }
    function Je(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        if(d>>>0>4294967279)wd(b);
        if(d>>>0<11)
        {
            a[b>>0]=d<<1;
            b=b+1|0
        }
        else
        {
            g=d+16&-16;
            f=Kc(g)|0;
            c[b+8>>2]=f;
            c[b>>2]=g|1;
            c[b+4>>2]=d;
            b=f
        }
        Xn(b|0,e|0,d|0)|0;
        a[b+d>>0]=0;
        return
    }
    function Ke(b)
    {
        b=b|0;
        if(a[b>>0]&1)Lc(c[b+8>>2]|0);
        return
    }
    function Le(a,b)
    {
        a=a|0;
        b=b|0;
        return Me(a,b,Yn(b|0)|0)|0
    }
    function Me(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0;
        f=a[b>>0]|0;
        if(!(f&1))h=10;
        else
        {
            f=c[b>>2]|0;
            h=(f&-2)+-1|0;
            f=f&255
        }
        g=(f&1)==0;
        do if(h>>>0>=e>>>0)
        {
            if(g)f=b+1|0;
            else f=c[b+8>>2]|0;
            ao(f|0,d|0,e|0)|0;
            a[f+e>>0]=0;
            if(!(a[b>>0]&1))
            {
                a[b>>0]=e<<1;
                break
            }
            else
            {
                c[b+4>>2]=e;
                break
            }
        }
        else
        {
            if(g)f=(f&255)>>>1;
            else f=c[b+4>>2]|0;
            Re(b,h,e-h|0,f,0,f,e,d)
        }
        while(0);
        return b|0
    }
    function Ne(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        f=a[b>>0]|0;
        g=(f&1)==0;
        if(g)f=(f&255)>>>1;
        else f=c[b+4>>2]|0;
        do if(f>>>0>=d>>>0)if(g)
        {
            a[b+1+d>>0]=0;
            a[b>>0]=d<<1;
            break
        }
        else
        {
            a[(c[b+8>>2]|0)+d>>0]=0;
            c[b+4>>2]=d;
            break
        }
        else Oe(b,d-f|0,e)|0;
        while(0);
        return
    }
    function Oe(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0;
        if(d)
        {
            f=a[b>>0]|0;
            if(!(f&1))g=10;
            else
            {
                f=c[b>>2]|0;
                g=(f&-2)+-1|0;
                f=f&255
            }
            if(!(f&1))h=(f&255)>>>1;
            else h=c[b+4>>2]|0;
            if((g-h|0)>>>0<d>>>0)
            {
                Se(b,g,d-g+h|0,h,h,0,0);
                f=a[b>>0]|0
            }
            if(!(f&1))g=b+1|0;
            else g=c[b+8>>2]|0;
            Xn(g+h|0,e|0,d|0)|0;
            f=h+d|0;
            if(!(a[b>>0]&1))a[b>>0]=f<<1;
            else c[b+4>>2]=f;
            a[g+f>>0]=0
        }
        return b|0
    }
    function Pe(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,i=0,j=0;
        if(d>>>0>4294967279)wd(b);
        e=a[b>>0]|0;
        if(!(e&1))f=10;
        else
        {
            e=c[b>>2]|0;
            f=(e&-2)+-1|0;
            e=e&255
        }
        if(!(e&1))j=(e&255)>>>1;
        else j=c[b+4>>2]|0;
        d=j>>>0>d>>>0?j:d;
        if(d>>>0<11)i=10;
        else i=(d+16&-16)+-1|0;
        do if((i|0)!=(f|0))
        {
            do if((i|0)!=10)
            {
                d=Kc(i+1|0)|0;
                if(!(e&1))
                {
                    f=1;
                    g=b+1|0;
                    h=0;
                    break
                }
                else
                {
                    f=1;
                    g=c[b+8>>2]|0;
                    h=1;
                    break
                }
            }
            else
            {
                d=b+1|0;
                f=0;
                g=c[b+8>>2]|0;
                h=1
            }
            while(0);
            if(!(e&1))e=(e&255)>>>1;
            else e=c[b+4>>2]|0;
            _n(d|0,g|0,e+1|0)|0;
            if(h)Lc(g);
            if(f)
            {
                c[b>>2]=i+1|1;
                c[b+4>>2]=j;
                c[b+8>>2]=d;
                break
            }
            else
            {
                a[b>>0]=j<<1;
                break
            }
        }
        while(0);
        return
    }
    function Qe(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0;
        e=a[b>>0]|0;
        f=(e&1)!=0;
        if(f)
        {
            g=(c[b>>2]&-2)+-1|0;
            h=c[b+4>>2]|0
        }
        else
        {
            g=10;
            h=(e&255)>>>1
        }
        if((h|0)==(g|0))
        {
            Se(b,g,1,g,g,0,0);
            if(!(a[b>>0]&1))g=7;
            else g=8
        }
        else if(f)g=8;
        else g=7;
        if((g|0)==7)
        {
            a[b>>0]=(h<<1)+2;
            e=b+1|0;
            f=h+1|0
        }
        else if((g|0)==8)
        {
            e=c[b+8>>2]|0;
            f=h+1|0;
            c[b+4>>2]=f
        }
        a[e+h>>0]=d;
        a[e+f>>0]=0;
        return
    }
    function Re(b,d,e,f,g,h,i,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        var k=0,l=0,m=0;
        if((-18-d|0)>>>0<e>>>0)wd(b);
        if(!(a[b>>0]&1))m=b+1|0;
        else m=c[b+8>>2]|0;
        if(d>>>0<2147483623)
        {
            k=e+d|0;
            l=d<<1;
            k=k>>>0<l>>>0?l:k;
            k=k>>>0<11?11:k+16&-16
        }
        else k=-17;
        l=Kc(k)|0;
        if(g)_n(l|0,m|0,g|0)|0;
        if(i)_n(l+g|0,j|0,i|0)|0;
        e=f-h|0;
        if((e|0)!=(g|0))_n(l+(i+g)|0,m+(h+g)|0,e-g|0)|0;
        if((d|0)!=10)Lc(m);
        c[b+8>>2]=l;
        c[b>>2]=k|1;
        d=e+i|0;
        c[b+4>>2]=d;
        a[l+d>>0]=0;
        return
    }
    function Se(b,d,e,f,g,h,i)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        var j=0,k=0,l=0;
        if((-17-d|0)>>>0<e>>>0)wd(b);
        if(!(a[b>>0]&1))l=b+1|0;
        else l=c[b+8>>2]|0;
        if(d>>>0<2147483623)
        {
            j=e+d|0;
            k=d<<1;
            j=j>>>0<k>>>0?k:j;
            j=j>>>0<11?11:j+16&-16
        }
        else j=-17;
        k=Kc(j)|0;
        if(g)_n(k|0,l|0,g|0)|0;
        e=f-h|0;
        if((e|0)!=(g|0))_n(k+(i+g)|0,l+(h+g)|0,e-g|0)|0;
        if((d|0)!=10)Lc(l);
        c[b+8>>2]=k;
        c[b>>2]=j|1;
        return
    }
    function Te(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        if(e>>>0>1073741807)wd(b);
        if(e>>>0<2)
        {
            a[b>>0]=e<<1;
            b=b+4|0
        }
        else
        {
            g=e+4&-4;
            f=Kc(g<<2)|0;
            c[b+8>>2]=f;
            c[b>>2]=g|1;
            c[b+4>>2]=e;
            b=f
        }
        Dc(b,d,e)|0;
        c[b+(e<<2)>>2]=0;
        return
    }
    function Ue(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        if(d>>>0>1073741807)wd(b);
        if(d>>>0<2)
        {
            a[b>>0]=d<<1;
            b=b+4|0
        }
        else
        {
            g=d+4&-4;
            f=Kc(g<<2)|0;
            c[b+8>>2]=f;
            c[b>>2]=g|1;
            c[b+4>>2]=d;
            b=f
        }
        Fc(b,e,d)|0;
        c[b+(d<<2)>>2]=0;
        return
    }
    function Ve(b)
    {
        b=b|0;
        if(a[b>>0]&1)Lc(c[b+8>>2]|0);
        return
    }
    function We(a,b)
    {
        a=a|0;
        b=b|0;
        return Xe(a,b,Cc(b)|0)|0
    }
    function Xe(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0;
        f=a[b>>0]|0;
        if(!(f&1))h=1;
        else
        {
            f=c[b>>2]|0;
            h=(f&-2)+-1|0;
            f=f&255
        }
        g=(f&1)==0;
        do if(h>>>0>=e>>>0)
        {
            if(g)f=b+4|0;
            else f=c[b+8>>2]|0;
            Ec(f,d,e)|0;
            c[f+(e<<2)>>2]=0;
            if(!(a[b>>0]&1))
            {
                a[b>>0]=e<<1;
                break
            }
            else
            {
                c[b+4>>2]=e;
                break
            }
        }
        else
        {
            if(g)f=(f&255)>>>1;
            else f=c[b+4>>2]|0;
            _e(b,h,e-h|0,f,0,f,e,d)
        }
        while(0);
        return b|0
    }
    function Ye(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,i=0,j=0;
        if(d>>>0>1073741807)wd(b);
        e=a[b>>0]|0;
        if(!(e&1))f=1;
        else
        {
            e=c[b>>2]|0;
            f=(e&-2)+-1|0;
            e=e&255
        }
        if(!(e&1))j=(e&255)>>>1;
        else j=c[b+4>>2]|0;
        d=j>>>0>d>>>0?j:d;
        if(d>>>0<2)i=1;
        else i=(d+4&-4)+-1|0;
        do if((i|0)!=(f|0))
        {
            do if((i|0)!=1)
            {
                d=Kc((i<<2)+4|0)|0;
                if(!(e&1))
                {
                    f=1;
                    g=b+4|0;
                    h=0;
                    break
                }
                else
                {
                    f=1;
                    g=c[b+8>>2]|0;
                    h=1;
                    break
                }
            }
            else
            {
                d=b+4|0;
                f=0;
                g=c[b+8>>2]|0;
                h=1
            }
            while(0);
            if(!(e&1))e=(e&255)>>>1;
            else e=c[b+4>>2]|0;
            Dc(d,g,e+1|0)|0;
            if(h)Lc(g);
            if(f)
            {
                c[b>>2]=i+1|1;
                c[b+4>>2]=j;
                c[b+8>>2]=d;
                break
            }
            else
            {
                a[b>>0]=j<<1;
                break
            }
        }
        while(0);
        return
    }
    function Ze(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0;
        e=a[b>>0]|0;
        f=(e&1)!=0;
        if(f)
        {
            g=(c[b>>2]&-2)+-1|0;
            h=c[b+4>>2]|0
        }
        else
        {
            g=1;
            h=(e&255)>>>1
        }
        if((h|0)==(g|0))
        {
            $e(b,g,1,g,g,0,0);
            if(!(a[b>>0]&1))g=7;
            else g=8
        }
        else if(f)g=8;
        else g=7;
        if((g|0)==7)
        {
            a[b>>0]=(h<<1)+2;
            e=b+4|0;
            f=h+1|0
        }
        else if((g|0)==8)
        {
            e=c[b+8>>2]|0;
            f=h+1|0;
            c[b+4>>2]=f
        }
        c[e+(h<<2)>>2]=d;
        c[e+(f<<2)>>2]=0;
        return
    }
    function _e(b,d,e,f,g,h,i,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        var k=0,l=0,m=0;
        if((1073741806-d|0)>>>0<e>>>0)wd(b);
        if(!(a[b>>0]&1))m=b+4|0;
        else m=c[b+8>>2]|0;
        if(d>>>0<536870887)
        {
            k=e+d|0;
            l=d<<1;
            k=k>>>0<l>>>0?l:k;
            k=k>>>0<2?2:k+4&-4
        }
        else k=1073741807;
        l=Kc(k<<2)|0;
        if(g)Dc(l,m,g)|0;
        if(i)Dc(l+(g<<2)|0,j,i)|0;
        e=f-h|0;
        if((e|0)!=(g|0))Dc(l+(i+g<<2)|0,m+(h+g<<2)|0,e-g|0)|0;
        if((d|0)!=1)Lc(m);
        c[b+8>>2]=l;
        c[b>>2]=k|1;
        d=e+i|0;
        c[b+4>>2]=d;
        c[l+(d<<2)>>2]=0;
        return
    }
    function $e(b,d,e,f,g,h,i)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        var j=0,k=0,l=0;
        if((1073741807-d|0)>>>0<e>>>0)wd(b);
        if(!(a[b>>0]&1))l=b+4|0;
        else l=c[b+8>>2]|0;
        if(d>>>0<536870887)
        {
            j=e+d|0;
            k=d<<1;
            j=j>>>0<k>>>0?k:j;
            j=j>>>0<2?2:j+4&-4
        }
        else j=1073741807;
        k=Kc(j<<2)|0;
        if(g)Dc(k,l,g)|0;
        e=f-h|0;
        if((e|0)!=(g|0))Dc(k+(i+g<<2)|0,l+(h+g<<2)|0,e-g|0)|0;
        if((d|0)!=1)Lc(l);
        c[b+8>>2]=k;
        c[b>>2]=j|1;
        return
    }
    function af(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,i=0;
        g=d;
        f=e-g|0;
        if(f>>>0>4294967279)wd(b);
        if(f>>>0<11)
        {
            a[b>>0]=f<<1;
            b=b+1|0
        }
        else
        {
            i=f+16&-16;
            h=Kc(i)|0;
            c[b+8>>2]=h;
            c[b>>2]=i|1;
            c[b+4>>2]=f;
            b=h
        }
        g=e-g|0;
        if((d|0)!=(e|0))
        {
            f=b;
            while(1)
            {
                a[f>>0]=a[d>>0]|0;
                d=d+1|0;
                if((d|0)==(e|0))break;
                else f=f+1|0
            }
        }
        a[b+g>>0]=0;
        return
    }
    function bf(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,i=0;
        h=d;
        f=e-h|0;
        g=f>>2;
        if(g>>>0>1073741807)wd(b);
        if(g>>>0<2)
        {
            a[b>>0]=f>>>1;
            b=b+4|0
        }
        else
        {
            i=g+4&-4;
            f=Kc(i<<2)|0;
            c[b+8>>2]=f;
            c[b>>2]=i|1;
            c[b+4>>2]=g;
            b=f
        }
        g=(e-h|0)>>>2;
        if((d|0)!=(e|0))
        {
            f=b;
            while(1)
            {
                c[f>>2]=c[d>>2];
                d=d+4|0;
                if((d|0)==(e|0))break;
                else f=f+4|0
            }
        }
        c[b+(g<<2)>>2]=0;
        return
    }
    function cf(a,b)
    {
        a=a|0;
        b=b|0;
        c[a+16>>2]=(c[a+24>>2]|0)==0|b;
        return
    }
    function df(a)
    {
        a=a|0;
        c[a>>2]=4816;
        ef(a,0);
        rk(a+28|0);
        ae(c[a+32>>2]|0);
        ae(c[a+36>>2]|0);
        ae(c[a+48>>2]|0);
        ae(c[a+60>>2]|0);
        return
    }
    function ef(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0,f=0;
        d=c[a+40>>2]|0;
        e=a+32|0;
        f=a+36|0;
        if(d)do
            {
            d=d+-1|0;
            Rb[c[(c[e>>2]|0)+(d<<2)>>2]&0](b,a,c[(c[f>>2]|0)+(d<<2)>>2]|0)
        }
        while((d|0)!=0);
        return
    }
    function ff(a)
    {
        a=a|0;
        var b=0,d=0;
        d=i;
        i=i+16|0;
        b=d;
        qk(b,a+28|0);
        i=d;
        return c[b>>2]|0
    }
    function gf(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0;
        c[a+24>>2]=b;
        c[a+16>>2]=(b|0)==0&1;
        c[a+20>>2]=0;
        c[a+4>>2]=4098;
        c[a+12>>2]=0;
        c[a+8>>2]=6;
        d=a+28|0;
        b=a+32|0;
        a=b+40|0;
        do
        {
            c[b>>2]=0;
            b=b+4|0
        }
        while((b|0)<(a|0));
        pk(d);
        return
    }
    function hf(a)
    {
        a=a|0;
        c[a>>2]=4528;
        rk(a+4|0);
        return
    }
    function jf(a)
    {
        a=a|0;
        c[a>>2]=4528;
        rk(a+4|0);
        Lc(a);
        return
    }
    function kf(a)
    {
        a=a|0;
        c[a>>2]=4528;
        pk(a+4|0);
        a=a+8|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        c[a+12>>2]=0;
        c[a+16>>2]=0;
        c[a+20>>2]=0;
        return
    }
    function lf(a,b)
    {
        a=a|0;
        b=b|0;
        return
    }
    function mf(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        return a|0
    }
    function nf(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        b=a;
        c[b>>2]=0;
        c[b+4>>2]=0;
        b=a+8|0;
        c[b>>2]=-1;
        c[b+4>>2]=-1;
        return
    }
    function of(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        b=a;
        c[b>>2]=0;
        c[b+4>>2]=0;
        a=a+8|0;
        c[a>>2]=-1;
        c[a+4>>2]=-1;
        return
    }
    function pf(a)
    {
        a=a|0;
        return 0
    }
    function qf(a)
    {
        a=a|0;
        return 0
    }
    function rf(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,i=0;
        h=b+12|0;
        i=b+16|0;
        a:do if((e|0)>0)
        {
            g=d;
            d=0;
            while(1)
            {
                f=c[h>>2]|0;
                if(f>>>0<(c[i>>2]|0)>>>0)
                {
                    c[h>>2]=f+1;
                    f=a[f>>0]|0
                }
                else
                {
                    f=Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                    if((f|0)==-1)break a;
                    f=f&255
                }
                a[g>>0]=f;
                d=d+1|0;
                if((d|0)<(e|0))g=g+1|0;
                else break
            }
        }
        else d=0;
        while(0);
        return d|0
    }
    function sf(a)
    {
        a=a|0;
        return -1
    }
    function tf(a)
    {
        a=a|0;
        var b=0;
        if((Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0)==-1)a=-1;
        else
        {
            b=a+12|0;
            a=c[b>>2]|0;
            c[b>>2]=a+1;
            a=d[a>>0]|0
        }
        return a|0
    }
    function uf(a,b)
    {
        a=a|0;
        b=b|0;
        return -1
    }
    function vf(b,e,f)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,i=0,j=0,k=0;
        i=b+24|0;
        j=b+28|0;
        a:do if((f|0)>0)
        {
            h=e;
            e=0;
            while(1)
            {
                g=c[i>>2]|0;
                if(g>>>0>=(c[j>>2]|0)>>>0)
                {
                    if((Wb[c[(c[b>>2]|0)+52>>2]&15](b,d[h>>0]|0)|0)==-1)break a
                }
                else
                {
                    k=a[h>>0]|0;
                    c[i>>2]=g+1;
                    a[g>>0]=k
                }
                e=e+1|0;
                if((e|0)<(f|0))h=h+1|0;
                else break
            }
        }
        else e=0;
        while(0);
        return e|0
    }
    function wf(a,b)
    {
        a=a|0;
        b=b|0;
        return -1
    }
    function xf(a)
    {
        a=a|0;
        c[a>>2]=4592;
        rk(a+4|0);
        return
    }
    function yf(a)
    {
        a=a|0;
        c[a>>2]=4592;
        rk(a+4|0);
        Lc(a);
        return
    }
    function zf(a)
    {
        a=a|0;
        c[a>>2]=4592;
        pk(a+4|0);
        a=a+8|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        c[a+12>>2]=0;
        c[a+16>>2]=0;
        c[a+20>>2]=0;
        return
    }
    function Af(a,b)
    {
        a=a|0;
        b=b|0;
        return
    }
    function Bf(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        return a|0
    }
    function Cf(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        b=a;
        c[b>>2]=0;
        c[b+4>>2]=0;
        b=a+8|0;
        c[b>>2]=-1;
        c[b+4>>2]=-1;
        return
    }
    function Df(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        b=a;
        c[b>>2]=0;
        c[b+4>>2]=0;
        a=a+8|0;
        c[a>>2]=-1;
        c[a+4>>2]=-1;
        return
    }
    function Ef(a)
    {
        a=a|0;
        return 0
    }
    function Ff(a)
    {
        a=a|0;
        return 0
    }
    function Gf(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0;
        g=a+12|0;
        h=a+16|0;
        a:do if((d|0)>0)
        {
            f=b;
            b=0;
            while(1)
            {
                e=c[g>>2]|0;
                if(e>>>0>=(c[h>>2]|0)>>>0)
                {
                    e=Qb[c[(c[a>>2]|0)+40>>2]&63](a)|0;
                    if((e|0)==-1)break a
                }
                else
                {
                    c[g>>2]=e+4;
                    e=c[e>>2]|0
                }
                c[f>>2]=e;
                b=b+1|0;
                if((b|0)<(d|0))f=f+4|0;
                else break
            }
        }
        else b=0;
        while(0);
        return b|0
    }
    function Hf(a)
    {
        a=a|0;
        return -1
    }
    function If(a)
    {
        a=a|0;
        var b=0;
        if((Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0)==-1)a=-1;
        else
        {
            b=a+12|0;
            a=c[b>>2]|0;
            c[b>>2]=a+4;
            a=c[a>>2]|0
        }
        return a|0
    }
    function Jf(a,b)
    {
        a=a|0;
        b=b|0;
        return -1
    }
    function Kf(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,i=0;
        g=a+24|0;
        h=a+28|0;
        a:do if((d|0)>0)
        {
            f=b;
            b=0;
            while(1)
            {
                e=c[g>>2]|0;
                if(e>>>0>=(c[h>>2]|0)>>>0)
                {
                    if((Wb[c[(c[a>>2]|0)+52>>2]&15](a,c[f>>2]|0)|0)==-1)break a
                }
                else
                {
                    i=c[f>>2]|0;
                    c[g>>2]=e+4;
                    c[e>>2]=i
                }
                b=b+1|0;
                if((b|0)<(d|0))f=f+4|0;
                else break
            }
        }
        else b=0;
        while(0);
        return b|0
    }
    function Lf(a,b)
    {
        a=a|0;
        b=b|0;
        return -1
    }
    function Mf(a)
    {
        a=a|0;
        df(a+8|0);
        return
    }
    function Nf(a)
    {
        a=a|0;
        df(a+((c[(c[a>>2]|0)+-12>>2]|0)+8)|0);
        return
    }
    function Of(a)
    {
        a=a|0;
        df(a+8|0);
        Lc(a);
        return
    }
    function Pf(a)
    {
        a=a|0;
        Of(a+(c[(c[a>>2]|0)+-12>>2]|0)|0);
        return
    }
    function Qf(b)
    {
        b=b|0;
        var d=0,e=0,f=0;
        e=i;
        i=i+16|0;
        d=e;
        if(c[b+((c[(c[b>>2]|0)+-12>>2]|0)+24)>>2]|0)
        {
            _f(d,b);
            if((a[d>>0]|0)!=0?(f=c[b+((c[(c[b>>2]|0)+-12>>2]|0)+24)>>2]|0,(Qb[c[(c[f>>2]|0)+24>>2]&63](f)|0)==-1):0)
            {
                f=b+((c[(c[b>>2]|0)+-12>>2]|0)+16)|0;
                c[f>>2]=c[f>>2]|1
            }
            $f(d)
        }
        i=e;
        return b|0
    }
    function Rf(a)
    {
        a=a|0;
        df(a+8|0);
        return
    }
    function Sf(a)
    {
        a=a|0;
        df(a+((c[(c[a>>2]|0)+-12>>2]|0)+8)|0);
        return
    }
    function Tf(a)
    {
        a=a|0;
        df(a+8|0);
        Lc(a);
        return
    }
    function Uf(a)
    {
        a=a|0;
        Tf(a+(c[(c[a>>2]|0)+-12>>2]|0)|0);
        return
    }
    function Vf(b)
    {
        b=b|0;
        var d=0,e=0,f=0;
        e=i;
        i=i+16|0;
        d=e;
        if(c[b+((c[(c[b>>2]|0)+-12>>2]|0)+24)>>2]|0)
        {
            gg(d,b);
            if((a[d>>0]|0)!=0?(f=c[b+((c[(c[b>>2]|0)+-12>>2]|0)+24)>>2]|0,(Qb[c[(c[f>>2]|0)+24>>2]&63](f)|0)==-1):0)
            {
                f=b+((c[(c[b>>2]|0)+-12>>2]|0)+16)|0;
                c[f>>2]=c[f>>2]|1
            }
            hg(d)
        }
        i=e;
        return b|0
    }
    function Wf(a)
    {
        a=a|0;
        df(a+4|0);
        return
    }
    function Xf(a)
    {
        a=a|0;
        df(a+((c[(c[a>>2]|0)+-12>>2]|0)+4)|0);
        return
    }
    function Yf(a)
    {
        a=a|0;
        df(a+4|0);
        Lc(a);
        return
    }
    function Zf(a)
    {
        a=a|0;
        Yf(a+(c[(c[a>>2]|0)+-12>>2]|0)|0);
        return
    }
    function _f(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0;
        a[b>>0]=0;
        c[b+4>>2]=d;
        e=c[(c[d>>2]|0)+-12>>2]|0;
        if(!(c[d+(e+16)>>2]|0))
        {
            e=c[d+(e+72)>>2]|0;
            if(e)Qf(e)|0;
            a[b>>0]=1
        }
        return
    }
    function $f(a)
    {
        a=a|0;
        var b=0,d=0;
        a=a+4|0;
        d=c[a>>2]|0;
        b=c[(c[d>>2]|0)+-12>>2]|0;
        if(((((c[d+(b+24)>>2]|0)!=0?(c[d+(b+16)>>2]|0)==0:0)?(c[d+(b+4)>>2]&8192|0)!=0:0)?!(ab()|0):0)?(d=c[a>>2]|0,d=c[d+((c[(c[d>>2]|0)+-12>>2]|0)+24)>>2]|0,(Qb[c[(c[d>>2]|0)+24>>2]&63](d)|0)==-1):0)
        {
            d=c[a>>2]|0;
            d=d+((c[(c[d>>2]|0)+-12>>2]|0)+16)|0;
            c[d>>2]=c[d>>2]|1
        }
        return
    }
    function ag(b,d)
    {
        b=b|0;
        d=+d;
        var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0;
        n=i;
        i=i+32|0;
        j=n+20|0;
        k=n;
        m=n+8|0;
        e=n+16|0;
        _f(m,b);
        if(a[m>>0]|0)
        {
            c[e>>2]=ff(b+(c[(c[b>>2]|0)+-12>>2]|0)|0)|0;
            l=tk(e,5720)|0;
            rk(e);
            f=c[(c[b>>2]|0)+-12>>2]|0;
            g=c[b+(f+24)>>2]|0;
            h=b+f|0;
            f=b+(f+76)|0;
            e=c[f>>2]|0;
            if((e|0)==-1)
            {
                c[j>>2]=ff(h)|0;
                e=tk(j,6584)|0;
                e=Wb[c[(c[e>>2]|0)+28>>2]&15](e,32)|0;
                rk(j);
                e=e<<24>>24;
                c[f>>2]=e
            }
            f=c[(c[l>>2]|0)+32>>2]|0;
            c[k>>2]=g;
            c[j>>2]=c[k>>2];
            if(!(Pb[f&7](l,j,h,e&255,d)|0))
            {
                l=b+((c[(c[b>>2]|0)+-12>>2]|0)+16)|0;
                c[l>>2]=c[l>>2]|5
            }
        }
        $f(m);
        i=n;
        return b|0
    }
    function bg(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0,k=0;
        k=i;
        i=i+16|0;
        j=k;
        _f(j,b);
        a:do if(a[j>>0]|0)
        {
            f=c[b+((c[(c[b>>2]|0)+-12>>2]|0)+24)>>2]|0;
            g=f;
            do if(f)
            {
                h=g+24|0;
                e=c[h>>2]|0;
                if((e|0)==(c[g+28>>2]|0))if((Wb[c[(c[f>>2]|0)+52>>2]&15](g,d&255)|0)==-1)break;
                else break a;
                else
                {
                    c[h>>2]=e+1;
                    a[e>>0]=d;
                    break a
                }
            }
            while(0);
            d=b+((c[(c[b>>2]|0)+-12>>2]|0)+16)|0;
            c[d>>2]=c[d>>2]|1
        }
        while(0);
        $f(j);
        i=k;
        return b|0
    }
    function cg(a)
    {
        a=a|0;
        df(a+4|0);
        return
    }
    function dg(a)
    {
        a=a|0;
        df(a+((c[(c[a>>2]|0)+-12>>2]|0)+4)|0);
        return
    }
    function eg(a)
    {
        a=a|0;
        df(a+4|0);
        Lc(a);
        return
    }
    function fg(a)
    {
        a=a|0;
        eg(a+(c[(c[a>>2]|0)+-12>>2]|0)|0);
        return
    }
    function gg(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0;
        a[b>>0]=0;
        c[b+4>>2]=d;
        e=c[(c[d>>2]|0)+-12>>2]|0;
        if(!(c[d+(e+16)>>2]|0))
        {
            e=c[d+(e+72)>>2]|0;
            if(e)Vf(e)|0;
            a[b>>0]=1
        }
        return
    }
    function hg(a)
    {
        a=a|0;
        var b=0,d=0;
        a=a+4|0;
        d=c[a>>2]|0;
        b=c[(c[d>>2]|0)+-12>>2]|0;
        if(((((c[d+(b+24)>>2]|0)!=0?(c[d+(b+16)>>2]|0)==0:0)?(c[d+(b+4)>>2]&8192|0)!=0:0)?!(ab()|0):0)?(d=c[a>>2]|0,d=c[d+((c[(c[d>>2]|0)+-12>>2]|0)+24)>>2]|0,(Qb[c[(c[d>>2]|0)+24>>2]&63](d)|0)==-1):0)
        {
            d=c[a>>2]|0;
            d=d+((c[(c[d>>2]|0)+-12>>2]|0)+16)|0;
            c[d>>2]=c[d>>2]|1
        }
        return
    }
    function ig(a)
    {
        a=a|0;
        df(a);
        Lc(a);
        return
    }
    function jg(a)
    {
        a=a|0;
        a=a+16|0;
        c[a>>2]=c[a>>2]|1;
        return
    }
    function kg(a)
    {
        a=a|0;
        return
    }
    function lg(a)
    {
        a=a|0;
        return
    }
    function mg(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function ng(b,c,d,e,f)
    {
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0;
        a:do if((e|0)==(f|0))h=6;
        else while(1)
            {
            if((c|0)==(d|0))
            {
                c=-1;
                break a
            }
            b=a[c>>0]|0;
            g=a[e>>0]|0;
            if(b<<24>>24<g<<24>>24)
            {
                c=-1;
                break a
            }
            if(g<<24>>24<b<<24>>24)
            {
                c=1;
                break a
            }
            c=c+1|0;
            e=e+1|0;
            if((e|0)==(f|0))
            {
                h=6;
                break
            }
        }
        while(0);
        if((h|0)==6)c=(c|0)!=(d|0)&1;
        return c|0
    }
    function og(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        af(a,c,d);
        return
    }
    function pg(b,c,d)
    {
        b=b|0;
        c=c|0;
        d=d|0;
        var e=0;
        if((c|0)==(d|0))b=0;
        else
        {
            b=0;
            do
            {
                b=(a[c>>0]|0)+(b<<4)|0;
                e=b&-268435456;
                b=(e>>>24|e)^b;
                c=c+1|0
            }
            while((c|0)!=(d|0))
        }
        return b|0
    }
    function qg(a)
    {
        a=a|0;
        return
    }
    function rg(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function sg(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0;
        a:do if((e|0)==(f|0))h=6;
        else while(1)
            {
            if((b|0)==(d|0))
            {
                b=-1;
                break a
            }
            a=c[b>>2]|0;
            g=c[e>>2]|0;
            if((a|0)<(g|0))
            {
                b=-1;
                break a
            }
            if((g|0)<(a|0))
            {
                b=1;
                break a
            }
            b=b+4|0;
            e=e+4|0;
            if((e|0)==(f|0))
            {
                h=6;
                break
            }
        }
        while(0);
        if((h|0)==6)b=(b|0)!=(d|0)&1;
        return b|0
    }
    function tg(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        bf(a,c,d);
        return
    }
    function ug(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0;
        if((b|0)==(d|0))a=0;
        else
        {
            a=0;
            do
            {
                a=(c[b>>2]|0)+(a<<4)|0;
                e=a&-268435456;
                a=(e>>>24|e)^a;
                b=b+4|0
            }
            while((b|0)!=(d|0))
        }
        return a|0
    }
    function vg(a)
    {
        a=a|0;
        return
    }
    function wg(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function xg(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;
        s=i;
        i=i+64|0;
        k=s+52|0;
        j=s+48|0;
        r=s+44|0;
        l=s+40|0;
        m=s+36|0;
        n=s+28|0;
        o=s+24|0;
        q=s;
        p=s+32|0;
        do if(!(c[f+4>>2]&1))
        {
            c[r>>2]=-1;
            q=c[(c[b>>2]|0)+16>>2]|0;
            c[l>>2]=c[d>>2];
            c[m>>2]=c[e>>2];
            c[j>>2]=c[l>>2];
            c[k>>2]=c[m>>2];
            j=Ob[q&63](b,j,k,f,g,r)|0;
            c[d>>2]=j;
            k=c[r>>2]|0;
            if(!k)
            {
                a[h>>0]=0;
                break
            }
            else if((k|0)==1)
            {
                a[h>>0]=1;
                break
            }
            else
            {
                a[h>>0]=1;
                c[g>>2]=4;
                break
            }
        }
        else
        {
            b=ff(f)|0;
            c[n>>2]=b;
            j=tk(n,6584)|0;
            Sn(b)|0;
            b=ff(f)|0;
            c[o>>2]=b;
            r=tk(o,6728)|0;
            Sn(b)|0;
            Nb[c[(c[r>>2]|0)+24>>2]&63](q,r);
            Nb[c[(c[r>>2]|0)+28>>2]&63](q+12|0,r);
            c[p>>2]=c[e>>2];
            c[k>>2]=c[p>>2];
            a[h>>0]=(fm(d,k,q,q+24|0,j,g,1)|0)==(q|0)&1;
            j=c[d>>2]|0;
            Ke(q+12|0);
            Ke(q)
        }
        while(0);
        i=s;
        return j|0
    }
    function yg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=gm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function zg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=hm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Ag(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=im(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Bg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=jm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Cg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=km(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Dg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=lm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Eg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=mm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Fg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=nm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Gg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=om(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Hg(b,e,f,g,h,j)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;
        z=i;
        i=i+240|0;
        w=z;
        p=z+208|0;
        y=z+184|0;
        t=z+180|0;
        x=z+196|0;
        v=z+168|0;
        r=z+8|0;
        s=z+172|0;
        q=z+176|0;
        c[y>>2]=0;
        c[y+4>>2]=0;
        c[y+8>>2]=0;
        u=ff(g)|0;
        c[t>>2]=u;
        t=tk(t,6584)|0;
        Ub[c[(c[t>>2]|0)+32>>2]&7](t,5552,5578,p)|0;
        Sn(u)|0;
        c[x>>2]=0;
        c[x+4>>2]=0;
        c[x+8>>2]=0;
        if(!(a[x>>0]&1))b=10;
        else b=(c[x>>2]&-2)+-1|0;
        Ne(x,b,0);
        t=x+8|0;
        u=x+1|0;
        g=(a[x>>0]&1)==0?u:c[t>>2]|0;
        c[v>>2]=g;
        c[s>>2]=r;
        c[q>>2]=0;
        o=x+4|0;
        b=c[e>>2]|0;
        a:while(1)
            {
            if(b)
            {
                if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
                {
                    c[e>>2]=0;
                    b=0
                }
            }
            else b=0;
            l=(b|0)==0;
            k=c[f>>2]|0;
            do if(k)
            {
                if((c[k+12>>2]|0)!=(c[k+16>>2]|0))if(l)break;
                else break a;
                if((Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0)!=-1)if(l)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    A=13;
                    break
                }
            }
            else A=13;
            while(0);
            if((A|0)==13)
            {
                A=0;
                if(l)
                {
                    k=0;
                    break
                }
                else k=0
            }
            m=a[x>>0]|0;
            m=(m&1)==0?(m&255)>>>1:c[o>>2]|0;
            if((c[v>>2]|0)==(g+m|0))
            {
                Ne(x,m<<1,0);
                if(!(a[x>>0]&1))l=10;
                else l=(c[x>>2]&-2)+-1|0;
                Ne(x,l,0);
                g=(a[x>>0]&1)==0?u:c[t>>2]|0;
                c[v>>2]=g+m
            }
            m=b+12|0;
            l=c[m>>2]|0;
            n=b+16|0;
            if((l|0)==(c[n>>2]|0))l=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else l=d[l>>0]|0;
            if(Ig(l&255,16,g,v,q,0,y,r,s,p)|0)break;
            l=c[m>>2]|0;
            if((l|0)==(c[n>>2]|0))
            {
                Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                continue
            }
            else
            {
                c[m>>2]=l+1;
                continue
            }
        }
        Ne(x,(c[v>>2]|0)-g|0,0);
        u=(a[x>>0]&1)==0?u:c[t>>2]|0;
        v=Jg()|0;
        c[w>>2]=j;
        if((pm(u,v,9544,w)|0)!=1)c[h>>2]=4;
        if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
            {
                c[e>>2]=0;
                b=0
            }
        }
        else b=0;
        b=(b|0)==0;
        do if(k)
        {
            if((c[k+12>>2]|0)==(c[k+16>>2]|0)?(Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0)==-1:0)
            {
                c[f>>2]=0;
                A=37;
                break
            }
            if(!b)A=38
        }
        else A=37;
        while(0);
        if((A|0)==37?b:0)A=38;
        if((A|0)==38)c[h>>2]=c[h>>2]|2;
        A=c[e>>2]|0;
        Ke(x);
        Ke(y);
        i=z;
        return A|0
    }
    function Ig(b,d,e,f,g,h,i,j,k,l)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        k=k|0;
        l=l|0;
        var m=0,n=0,o=0,p=0;
        o=c[f>>2]|0;
        p=(o|0)==(e|0);
        do if(p)
        {
            m=(a[l+24>>0]|0)==b<<24>>24;
            if(!m?(a[l+25>>0]|0)!=b<<24>>24:0)
            {
                n=5;
                break
            }
            c[f>>2]=e+1;
            a[e>>0]=m?43:45;
            c[g>>2]=0;
            m=0
        }
        else n=5;
        while(0);
        do if((n|0)==5)
        {
            n=a[i>>0]|0;
            if(b<<24>>24==h<<24>>24?(((n&1)==0?(n&255)>>>1:c[i+4>>2]|0)|0)!=0:0)
            {
                m=c[k>>2]|0;
                if((m-j|0)>=160)
                {
                    m=0;
                    break
                }
                d=c[g>>2]|0;
                c[k>>2]=m+4;
                c[m>>2]=d;
                c[g>>2]=0;
                m=0;
                break
            }
            m=l+26|0;
            i=l;
            do
            {
                if((a[i>>0]|0)==b<<24>>24)
                {
                    m=i;
                    break
                }
                i=i+1|0
            }
            while((i|0)!=(m|0));
            m=m-l|0;
            if((m|0)>23)m=-1;
            else
            {
                if((d|0)==10|(d|0)==8)
                {
                    if((m|0)>=(d|0))
                    {
                        m=-1;
                        break
                    }
                }
                else if((d|0)==16?(m|0)>=22:0)
                {
                    if(p)
                    {
                        m=-1;
                        break
                    }
                    if((o-e|0)>=3)
                    {
                        m=-1;
                        break
                    }
                    if((a[o+-1>>0]|0)!=48)
                    {
                        m=-1;
                        break
                    }
                    c[g>>2]=0;
                    m=a[5552+m>>0]|0;
                    c[f>>2]=o+1;
                    a[o>>0]=m;
                    m=0;
                    break
                }
                m=a[5552+m>>0]|0;
                c[f>>2]=o+1;
                a[o>>0]=m;
                c[g>>2]=(c[g>>2]|0)+1;
                m=0
            }
        }
        while(0);
        return m|0
    }
    function Jg()
    {
        if((a[9552]|0)==0?(Ea(9552)|0)!=0:0)
        {
            c[2392]=Xa(2147483647,9560,0)|0;
            Na(9552)
        }
        return c[2392]|0
    }
    function Kg(a)
    {
        a=a|0;
        return
    }
    function Lg(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Mg(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;
        s=i;
        i=i+64|0;
        k=s+52|0;
        j=s+48|0;
        r=s+44|0;
        l=s+40|0;
        m=s+36|0;
        n=s+28|0;
        o=s+24|0;
        q=s;
        p=s+32|0;
        do if(!(c[f+4>>2]&1))
        {
            c[r>>2]=-1;
            q=c[(c[b>>2]|0)+16>>2]|0;
            c[l>>2]=c[d>>2];
            c[m>>2]=c[e>>2];
            c[j>>2]=c[l>>2];
            c[k>>2]=c[m>>2];
            j=Ob[q&63](b,j,k,f,g,r)|0;
            c[d>>2]=j;
            k=c[r>>2]|0;
            if(!k)
            {
                a[h>>0]=0;
                break
            }
            else if((k|0)==1)
            {
                a[h>>0]=1;
                break
            }
            else
            {
                a[h>>0]=1;
                c[g>>2]=4;
                break
            }
        }
        else
        {
            b=ff(f)|0;
            c[n>>2]=b;
            j=tk(n,6576)|0;
            Sn(b)|0;
            b=ff(f)|0;
            c[o>>2]=b;
            r=tk(o,6736)|0;
            Sn(b)|0;
            Nb[c[(c[r>>2]|0)+24>>2]&63](q,r);
            Nb[c[(c[r>>2]|0)+28>>2]&63](q+12|0,r);
            c[p>>2]=c[e>>2];
            c[k>>2]=c[p>>2];
            a[h>>0]=(qm(d,k,q,q+24|0,j,g,1)|0)==(q|0)&1;
            j=c[d>>2]|0;
            Ve(q+12|0);
            Ve(q)
        }
        while(0);
        i=s;
        return j|0
    }
    function Ng(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=rm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Og(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=sm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Pg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=tm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Qg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=um(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Rg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=vm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Sg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=wm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Tg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=xm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Ug(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=ym(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Vg(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=zm(a,k,j,e,f,g)|0;
        i=h;
        return a|0
    }
    function Wg(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0;
        y=i;
        i=i+320|0;
        v=y;
        o=y+200|0;
        x=y+184|0;
        s=y+180|0;
        w=y+304|0;
        u=y+168|0;
        q=y+8|0;
        r=y+172|0;
        p=y+176|0;
        c[x>>2]=0;
        c[x+4>>2]=0;
        c[x+8>>2]=0;
        t=ff(f)|0;
        c[s>>2]=t;
        s=tk(s,6576)|0;
        Ub[c[(c[s>>2]|0)+48>>2]&7](s,5552,5578,o)|0;
        Sn(t)|0;
        c[w>>2]=0;
        c[w+4>>2]=0;
        c[w+8>>2]=0;
        if(!(a[w>>0]&1))b=10;
        else b=(c[w>>2]&-2)+-1|0;
        Ne(w,b,0);
        s=w+8|0;
        t=w+1|0;
        b=(a[w>>0]&1)==0?t:c[s>>2]|0;
        c[u>>2]=b;
        c[r>>2]=q;
        c[p>>2]=0;
        n=w+4|0;
        k=c[d>>2]|0;
        a:while(1)
            {
            if(k)
            {
                f=c[k+12>>2]|0;
                if((f|0)==(c[k+16>>2]|0))f=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else f=c[f>>2]|0;
                if((f|0)==-1)
                {
                    c[d>>2]=0;
                    f=0;
                    l=1
                }
                else
                {
                    f=k;
                    l=0
                }
            }
            else
            {
                f=0;
                l=1
            }
            j=c[e>>2]|0;
            do if(j)
            {
                k=c[j+12>>2]|0;
                if((k|0)==(c[j+16>>2]|0))k=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
                else k=c[k>>2]|0;
                if((k|0)!=-1)if(l)break;
                else break a;
                else
                {
                    c[e>>2]=0;
                    z=16;
                    break
                }
            }
            else z=16;
            while(0);
            if((z|0)==16)
            {
                z=0;
                if(l)
                {
                    j=0;
                    break
                }
                else j=0
            }
            k=a[w>>0]|0;
            k=(k&1)==0?(k&255)>>>1:c[n>>2]|0;
            if((c[u>>2]|0)==(b+k|0))
            {
                Ne(w,k<<1,0);
                if(!(a[w>>0]&1))b=10;
                else b=(c[w>>2]&-2)+-1|0;
                Ne(w,b,0);
                b=(a[w>>0]&1)==0?t:c[s>>2]|0;
                c[u>>2]=b+k
            }
            l=f+12|0;
            k=c[l>>2]|0;
            m=f+16|0;
            if((k|0)==(c[m>>2]|0))k=Qb[c[(c[f>>2]|0)+36>>2]&63](f)|0;
            else k=c[k>>2]|0;
            if(Xg(k,16,b,u,p,0,x,q,r,o)|0)break;
            k=c[l>>2]|0;
            if((k|0)==(c[m>>2]|0))
            {
                Qb[c[(c[f>>2]|0)+40>>2]&63](f)|0;
                k=f;
                continue
            }
            else
            {
                c[l>>2]=k+4;
                k=f;
                continue
            }
        }
        Ne(w,(c[u>>2]|0)-b|0,0);
        t=(a[w>>0]&1)==0?t:c[s>>2]|0;
        u=Jg()|0;
        c[v>>2]=h;
        if((pm(t,u,9544,v)|0)!=1)c[g>>2]=4;
        if(f)
        {
            b=c[f+12>>2]|0;
            if((b|0)==(c[f+16>>2]|0))b=Qb[c[(c[f>>2]|0)+36>>2]&63](f)|0;
            else b=c[b>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                f=1
            }
            else f=0
        }
        else f=1;
        do if(j)
        {
            b=c[j+12>>2]|0;
            if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(f)break;
            else
            {
                z=45;
                break
            }
            else
            {
                c[e>>2]=0;
                z=43;
                break
            }
        }
        else z=43;
        while(0);
        if((z|0)==43?f:0)z=45;
        if((z|0)==45)c[g>>2]=c[g>>2]|2;
        z=c[d>>2]|0;
        Ke(w);
        Ke(x);
        i=y;
        return z|0
    }
    function Xg(b,d,e,f,g,h,i,j,k,l)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        k=k|0;
        l=l|0;
        var m=0,n=0,o=0,p=0;
        o=c[f>>2]|0;
        p=(o|0)==(e|0);
        do if(p)
        {
            m=(c[l+96>>2]|0)==(b|0);
            if(!m?(c[l+100>>2]|0)!=(b|0):0)
            {
                n=5;
                break
            }
            c[f>>2]=e+1;
            a[e>>0]=m?43:45;
            c[g>>2]=0;
            m=0
        }
        else n=5;
        while(0);
        do if((n|0)==5)
        {
            n=a[i>>0]|0;
            if((b|0)==(h|0)?(((n&1)==0?(n&255)>>>1:c[i+4>>2]|0)|0)!=0:0)
            {
                m=c[k>>2]|0;
                if((m-j|0)>=160)
                {
                    m=0;
                    break
                }
                d=c[g>>2]|0;
                c[k>>2]=m+4;
                c[m>>2]=d;
                c[g>>2]=0;
                m=0;
                break
            }
            m=l+104|0;
            i=l;
            do
            {
                if((c[i>>2]|0)==(b|0))
                {
                    m=i;
                    break
                }
                i=i+4|0
            }
            while((i|0)!=(m|0));
            m=m-l|0;
            i=m>>2;
            if((m|0)>92)m=-1;
            else
            {
                if((d|0)==16)
                {
                    if((m|0)>=88)
                    {
                        if(p)
                        {
                            m=-1;
                            break
                        }
                        if((o-e|0)>=3)
                        {
                            m=-1;
                            break
                        }
                        if((a[o+-1>>0]|0)!=48)
                        {
                            m=-1;
                            break
                        }
                        c[g>>2]=0;
                        m=a[5552+i>>0]|0;
                        c[f>>2]=o+1;
                        a[o>>0]=m;
                        m=0;
                        break
                    }
                }
                else if((d|0)==10|(d|0)==8?(i|0)>=(d|0):0)
                {
                    m=-1;
                    break
                }
                m=a[5552+i>>0]|0;
                c[f>>2]=o+1;
                a[o>>0]=m;
                c[g>>2]=(c[g>>2]|0)+1;
                m=0
            }
        }
        while(0);
        return m|0
    }
    function Yg(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0;
        g=i;
        i=i+16|0;
        h=g;
        d=ff(d)|0;
        c[h>>2]=d;
        j=tk(h,6584)|0;
        Ub[c[(c[j>>2]|0)+32>>2]&7](j,5552,5578,e)|0;
        e=tk(h,6728)|0;
        a[f>>0]=Qb[c[(c[e>>2]|0)+16>>2]&63](e)|0;
        Nb[c[(c[e>>2]|0)+20>>2]&63](b,e);
        Sn(d)|0;
        i=g;
        return
    }
    function Zg(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0;
        h=i;
        i=i+16|0;
        j=h;
        d=ff(d)|0;
        c[j>>2]=d;
        k=tk(j,6584)|0;
        Ub[c[(c[k>>2]|0)+32>>2]&7](k,5552,5584,e)|0;
        e=tk(j,6728)|0;
        a[f>>0]=Qb[c[(c[e>>2]|0)+12>>2]&63](e)|0;
        a[g>>0]=Qb[c[(c[e>>2]|0)+16>>2]&63](e)|0;
        Nb[c[(c[e>>2]|0)+20>>2]&63](b,e);
        Sn(d)|0;
        i=h;
        return
    }
    function _g(b,e,f,g,h,i,j,k,l,m,n,o)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        n=n|0;
        o=o|0;
        var p=0,q=0;
        do if(b<<24>>24==i<<24>>24)if(a[e>>0]|0)
        {
            a[e>>0]=0;
            f=c[h>>2]|0;
            c[h>>2]=f+1;
            a[f>>0]=46;
            f=a[k>>0]|0;
            if((((f&1)==0?(f&255)>>>1:c[k+4>>2]|0)|0)!=0?(p=c[m>>2]|0,(p-l|0)<160):0)
            {
                l=c[n>>2]|0;
                c[m>>2]=p+4;
                c[p>>2]=l;
                p=0
            }
            else p=0
        }
        else p=-1;
        else
        {
            if(b<<24>>24==j<<24>>24?(j=a[k>>0]|0,(((j&1)==0?(j&255)>>>1:c[k+4>>2]|0)|0)!=0):0)
            {
                if(!(a[e>>0]|0))
                {
                    p=-1;
                    break
                }
                p=c[m>>2]|0;
                if((p-l|0)>=160)
                {
                    p=0;
                    break
                }
                l=c[n>>2]|0;
                c[m>>2]=p+4;
                c[p>>2]=l;
                c[n>>2]=0;
                p=0;
                break
            }
            p=o+32|0;
            i=o;
            do
            {
                if((a[i>>0]|0)==b<<24>>24)
                {
                    p=i;
                    break
                }
                i=i+1|0
            }
            while((i|0)!=(p|0));
            i=p-o|0;
            if((i|0)<=31)
            {
                j=a[5552+i>>0]|0;
                if((i|0)==24|(i|0)==25)
                {
                    p=c[h>>2]|0;
                    if((p|0)!=(g|0)?(d[p+-1>>0]&95|0)!=(d[f>>0]&127|0):0)
                    {
                        p=-1;
                        break
                    }
                    c[h>>2]=p+1;
                    a[p>>0]=j;
                    p=0;
                    break
                }
                else if((i|0)==23|(i|0)==22)
                {
                    a[f>>0]=80;
                    p=c[h>>2]|0;
                    c[h>>2]=p+1;
                    a[p>>0]=j;
                    p=0;
                    break
                }
                else
                {
                    p=j&95;
                    if((((p|0)==(a[f>>0]|0)?(a[f>>0]=p|128,(a[e>>0]|0)!=0):0)?(a[e>>0]=0,f=a[k>>0]|0,(((f&1)==0?(f&255)>>>1:c[k+4>>2]|0)|0)!=0):0)?(q=c[m>>2]|0,(q-l|0)<160):0)
                    {
                        l=c[n>>2]|0;
                        c[m>>2]=q+4;
                        c[q>>2]=l
                    }
                    m=c[h>>2]|0;
                    c[h>>2]=m+1;
                    a[m>>0]=j;
                    if((i|0)>21)
                    {
                        p=0;
                        break
                    }
                    c[n>>2]=(c[n>>2]|0)+1;
                    p=0;
                    break
                }
            }
            else p=-1
        }
        while(0);
        return p|0
    }
    function $g(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0;
        f=i;
        i=i+16|0;
        g=f;
        b=ff(b)|0;
        c[g>>2]=b;
        h=tk(g,6576)|0;
        Ub[c[(c[h>>2]|0)+48>>2]&7](h,5552,5578,d)|0;
        d=tk(g,6736)|0;
        c[e>>2]=Qb[c[(c[d>>2]|0)+16>>2]&63](d)|0;
        Nb[c[(c[d>>2]|0)+20>>2]&63](a,d);
        Sn(b)|0;
        i=f;
        return
    }
    function ah(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0;
        g=i;
        i=i+16|0;
        h=g;
        b=ff(b)|0;
        c[h>>2]=b;
        j=tk(h,6576)|0;
        Ub[c[(c[j>>2]|0)+48>>2]&7](j,5552,5584,d)|0;
        d=tk(h,6736)|0;
        c[e>>2]=Qb[c[(c[d>>2]|0)+12>>2]&63](d)|0;
        c[f>>2]=Qb[c[(c[d>>2]|0)+16>>2]&63](d)|0;
        Nb[c[(c[d>>2]|0)+20>>2]&63](a,d);
        Sn(b)|0;
        i=g;
        return
    }
    function bh(b,e,f,g,h,i,j,k,l,m,n,o)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        n=n|0;
        o=o|0;
        var p=0,q=0;
        do if((b|0)==(i|0))if(a[e>>0]|0)
        {
            a[e>>0]=0;
            f=c[h>>2]|0;
            c[h>>2]=f+1;
            a[f>>0]=46;
            f=a[k>>0]|0;
            if((((f&1)==0?(f&255)>>>1:c[k+4>>2]|0)|0)!=0?(p=c[m>>2]|0,(p-l|0)<160):0)
            {
                l=c[n>>2]|0;
                c[m>>2]=p+4;
                c[p>>2]=l;
                p=0
            }
            else p=0
        }
        else p=-1;
        else
        {
            if((b|0)==(j|0)?(j=a[k>>0]|0,(((j&1)==0?(j&255)>>>1:c[k+4>>2]|0)|0)!=0):0)
            {
                if(!(a[e>>0]|0))
                {
                    p=-1;
                    break
                }
                p=c[m>>2]|0;
                if((p-l|0)>=160)
                {
                    p=0;
                    break
                }
                l=c[n>>2]|0;
                c[m>>2]=p+4;
                c[p>>2]=l;
                c[n>>2]=0;
                p=0;
                break
            }
            p=o+128|0;
            i=o;
            do
            {
                if((c[i>>2]|0)==(b|0))
                {
                    p=i;
                    break
                }
                i=i+4|0
            }
            while((i|0)!=(p|0));
            i=p-o|0;
            p=i>>2;
            if((i|0)<=124)
            {
                j=a[5552+p>>0]|0;
                if((p|0)==24|(p|0)==25)
                {
                    p=c[h>>2]|0;
                    if((p|0)!=(g|0)?(d[p+-1>>0]&95|0)!=(d[f>>0]&127|0):0)
                    {
                        p=-1;
                        break
                    }
                    c[h>>2]=p+1;
                    a[p>>0]=j;
                    p=0;
                    break
                }
                else if(!((p|0)==23|(p|0)==22))
                {
                    p=j&95;
                    if((((p|0)==(a[f>>0]|0)?(a[f>>0]=p|128,(a[e>>0]|0)!=0):0)?(a[e>>0]=0,f=a[k>>0]|0,(((f&1)==0?(f&255)>>>1:c[k+4>>2]|0)|0)!=0):0)?(q=c[m>>2]|0,(q-l|0)<160):0)
                    {
                        l=c[n>>2]|0;
                        c[m>>2]=q+4;
                        c[q>>2]=l
                    }
                }
                else a[f>>0]=80;
                m=c[h>>2]|0;
                c[h>>2]=m+1;
                a[m>>0]=j;
                if((i|0)>84)p=0;
                else
                {
                    c[n>>2]=(c[n>>2]|0)+1;
                    p=0
                }
            }
            else p=-1
        }
        while(0);
        return p|0
    }
    function ch(a)
    {
        a=a|0;
        return
    }
    function dh(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function eh(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0;
        n=i;
        i=i+32|0;
        h=n+20|0;
        j=n;
        k=n+4|0;
        m=n+8|0;
        if(!(c[e+4>>2]&1))
        {
            l=c[(c[b>>2]|0)+24>>2]|0;
            c[j>>2]=c[d>>2];
            c[h>>2]=c[j>>2];
            h=Xb[l&31](b,h,e,f,g&1)|0
        }
        else
        {
            j=ff(e)|0;
            c[k>>2]=j;
            h=tk(k,6728)|0;
            Sn(j)|0;
            j=c[h>>2]|0;
            if(g)Nb[c[j+24>>2]&63](m,h);
            else Nb[c[j+28>>2]&63](m,h);
            b=a[m>>0]|0;
            g=(b&1)==0;
            h=m+1|0;
            l=m+8|0;
            e=g?h:m+1|0;
            h=g?h:c[m+8>>2]|0;
            g=m+4|0;
            k=(b&1)==0;
            if((h|0)!=((k?e:c[l>>2]|0)+(k?(b&255)>>>1:c[g>>2]|0)|0))do
                {
                f=a[h>>0]|0;
                j=c[d>>2]|0;
                do if(j)
                {
                    k=j+24|0;
                    b=c[k>>2]|0;
                    if((b|0)!=(c[j+28>>2]|0))
                    {
                        c[k>>2]=b+1;
                        a[b>>0]=f;
                        break
                    }
                    if((Wb[c[(c[j>>2]|0)+52>>2]&15](j,f&255)|0)==-1)c[d>>2]=0
                }
                while(0);
                h=h+1|0;
                b=a[m>>0]|0;
                k=(b&1)==0
            }
            while((h|0)!=((k?e:c[l>>2]|0)+(k?(b&255)>>>1:c[g>>2]|0)|0));
            h=c[d>>2]|0;
            Ke(m)
        }
        i=n;
        return h|0
    }
    function fh(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;
        h=i;
        i=i+64|0;
        k=h;
        o=h+56|0;
        q=h+44|0;
        j=h+20|0;
        m=h+12|0;
        b=h+8|0;
        n=h+4|0;
        l=h+16|0;
        a[o>>0]=a[9576]|0;
        a[o+1>>0]=a[9577]|0;
        a[o+2>>0]=a[9578]|0;
        a[o+3>>0]=a[9579]|0;
        a[o+4>>0]=a[9580]|0;
        a[o+5>>0]=a[9581]|0;
        gh(o+1|0,9584,1,c[e+4>>2]|0);
        p=Jg()|0;
        c[k>>2]=g;
        o=q+(Am(q,12,p,o,k)|0)|0;
        p=hh(q,o,e)|0;
        g=ff(e)|0;
        c[n>>2]=g;
        ih(q,p,o,j,m,b,n);
        Sn(g)|0;
        c[l>>2]=c[d>>2];
        d=c[m>>2]|0;
        b=c[b>>2]|0;
        c[k>>2]=c[l>>2];
        b=kc(k,j,d,b,e,f)|0;
        i=h;
        return b|0
    }
    function gh(b,c,d,e)
    {
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        if(e&2048)
        {
            a[b>>0]=43;
            b=b+1|0
        }
        if(e&512)
        {
            a[b>>0]=35;
            b=b+1|0
        }
        f=a[c>>0]|0;
        if(f<<24>>24)
        {
            g=c;
            while(1)
            {
                g=g+1|0;
                c=b+1|0;
                a[b>>0]=f;
                f=a[g>>0]|0;
                if(!(f<<24>>24))
                {
                    b=c;
                    break
                }
                else b=c
            }
        }
        f=e&74;
        do if((f|0)==8)if(!(e&16384))
        {
            a[b>>0]=120;
            break
        }
        else
        {
            a[b>>0]=88;
            break
        }
        else if((f|0)!=64)if(d)
        {
            a[b>>0]=100;
            break
        }
        else
        {
            a[b>>0]=117;
            break
        }
        else a[b>>0]=111;
        while(0);
        return
    }
    function hh(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0;
        e=c[e+4>>2]&176;
        do if((e|0)==16)
        {
            e=a[b>>0]|0;
            if(e<<24>>24==43|e<<24>>24==45)
            {
                b=b+1|0;
                break
            }
            if((d-b|0)>1&e<<24>>24==48?(d=a[b+1>>0]|0,d<<24>>24==88|d<<24>>24==120):0)b=b+2|0;
            else f=7
        }
        else if((e|0)==32)b=d;
        else f=7;
        while(0);
        return b|0
    }
    function ih(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;
        t=i;
        i=i+16|0;
        s=t;
        r=tk(j,6584)|0;
        l=tk(j,6728)|0;
        Nb[c[(c[l>>2]|0)+20>>2]&63](s,l);
        o=a[s>>0]|0;
        q=s+4|0;
        if(((o&1)==0?(o&255)>>>1:c[q>>2]|0)|0)
        {
            c[h>>2]=f;
            j=a[b>>0]|0;
            if(j<<24>>24==43|j<<24>>24==45)
            {
                o=Wb[c[(c[r>>2]|0)+28>>2]&15](r,j)|0;
                j=c[h>>2]|0;
                c[h>>2]=j+1;
                a[j>>0]=o;
                j=b+1|0
            }
            else j=b;
            if(((e-j|0)>1?(a[j>>0]|0)==48:0)?(k=j+1|0,o=a[k>>0]|0,o<<24>>24==88|o<<24>>24==120):0)
            {
                p=Wb[c[(c[r>>2]|0)+28>>2]&15](r,48)|0;
                o=c[h>>2]|0;
                c[h>>2]=o+1;
                a[o>>0]=p;
                o=Wb[c[(c[r>>2]|0)+28>>2]&15](r,a[k>>0]|0)|0;
                p=c[h>>2]|0;
                c[h>>2]=p+1;
                a[p>>0]=o;
                p=j+2|0
            }
            else p=j;
            if((p|0)!=(e|0)?(m=e+-1|0,p>>>0<m>>>0):0)
            {
                k=p;
                j=m;
                do
                {
                    o=a[k>>0]|0;
                    a[k>>0]=a[j>>0]|0;
                    a[j>>0]=o;
                    k=k+1|0;
                    j=j+-1|0
                }
                while(k>>>0<j>>>0)
            }
            k=Qb[c[(c[l>>2]|0)+16>>2]&63](l)|0;
            m=s+8|0;
            n=s+1|0;
            if(p>>>0<e>>>0)
            {
                j=0;
                l=0;
                o=p;
                while(1)
                {
                    u=a[((a[s>>0]&1)==0?n:c[m>>2]|0)+l>>0]|0;
                    if(u<<24>>24!=0&(j|0)==(u<<24>>24|0))
                    {
                        u=c[h>>2]|0;
                        c[h>>2]=u+1;
                        a[u>>0]=k;
                        u=a[s>>0]|0;
                        j=0;
                        l=(l>>>0<(((u&1)==0?(u&255)>>>1:c[q>>2]|0)+-1|0)>>>0&1)+l|0
                    }
                    v=Wb[c[(c[r>>2]|0)+28>>2]&15](r,a[o>>0]|0)|0;
                    u=c[h>>2]|0;
                    c[h>>2]=u+1;
                    a[u>>0]=v;
                    o=o+1|0;
                    if(o>>>0>=e>>>0)break;
                    else j=j+1|0
                }
            }
            j=f+(p-b)|0;
            k=c[h>>2]|0;
            if((j|0)!=(k|0))
            {
                k=k+-1|0;
                if(j>>>0<k>>>0)do
                    {
                    v=a[j>>0]|0;
                    a[j>>0]=a[k>>0]|0;
                    a[k>>0]=v;
                    j=j+1|0;
                    k=k+-1|0
                }
                while(j>>>0<k>>>0);
                j=c[h>>2]|0
            }
        }
        else
        {
            Ub[c[(c[r>>2]|0)+32>>2]&7](r,b,e,f)|0;
            j=f+(e-b)|0;
            c[h>>2]=j
        }
        c[g>>2]=(d|0)==(e|0)?j:f+(d-b)|0;
        Ke(s);
        i=t;
        return
    }
    function jh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;
        h=i;
        i=i+96|0;
        k=h+8|0;
        o=h;
        p=h+74|0;
        j=h+32|0;
        m=h+20|0;
        a=h+24|0;
        n=h+16|0;
        l=h+28|0;
        q=o;
        c[q>>2]=37;
        c[q+4>>2]=0;
        gh(o+1|0,9592,1,c[d+4>>2]|0);
        q=Jg()|0;
        r=k;
        c[r>>2]=f;
        c[r+4>>2]=g;
        f=p+(Am(p,22,q,o,k)|0)|0;
        o=hh(p,f,d)|0;
        g=ff(d)|0;
        c[n>>2]=g;
        ih(p,o,f,j,m,a,n);
        Sn(g)|0;
        c[l>>2]=c[b>>2];
        b=c[m>>2]|0;
        a=c[a>>2]|0;
        c[k>>2]=c[l>>2];
        a=kc(k,j,b,a,d,e)|0;
        i=h;
        return a|0
    }
    function kh(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;
        h=i;
        i=i+64|0;
        k=h;
        o=h+56|0;
        q=h+44|0;
        j=h+20|0;
        m=h+12|0;
        b=h+8|0;
        n=h+4|0;
        l=h+16|0;
        a[o>>0]=a[9576]|0;
        a[o+1>>0]=a[9577]|0;
        a[o+2>>0]=a[9578]|0;
        a[o+3>>0]=a[9579]|0;
        a[o+4>>0]=a[9580]|0;
        a[o+5>>0]=a[9581]|0;
        gh(o+1|0,9584,0,c[e+4>>2]|0);
        p=Jg()|0;
        c[k>>2]=g;
        o=q+(Am(q,12,p,o,k)|0)|0;
        p=hh(q,o,e)|0;
        g=ff(e)|0;
        c[n>>2]=g;
        ih(q,p,o,j,m,b,n);
        Sn(g)|0;
        c[l>>2]=c[d>>2];
        d=c[m>>2]|0;
        b=c[b>>2]|0;
        c[k>>2]=c[l>>2];
        b=kc(k,j,d,b,e,f)|0;
        i=h;
        return b|0
    }
    function lh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;
        h=i;
        i=i+112|0;
        k=h+8|0;
        o=h;
        p=h+75|0;
        j=h+32|0;
        m=h+20|0;
        a=h+24|0;
        n=h+16|0;
        l=h+28|0;
        q=o;
        c[q>>2]=37;
        c[q+4>>2]=0;
        gh(o+1|0,9592,0,c[d+4>>2]|0);
        q=Jg()|0;
        r=k;
        c[r>>2]=f;
        c[r+4>>2]=g;
        f=p+(Am(p,23,q,o,k)|0)|0;
        o=hh(p,f,d)|0;
        g=ff(d)|0;
        c[n>>2]=g;
        ih(p,o,f,j,m,a,n);
        Sn(g)|0;
        c[l>>2]=c[b>>2];
        b=c[m>>2]|0;
        a=c[a>>2]|0;
        c[k>>2]=c[l>>2];
        a=kc(k,j,b,a,d,e)|0;
        i=h;
        return a|0
    }
    function mh(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=+f;
        var g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0;
        v=i;
        i=i+160|0;
        p=v+68|0;
        l=v;
        j=v+16|0;
        g=v+24|0;
        k=v+40|0;
        n=v+72|0;
        m=v+52|0;
        o=v+102|0;
        u=v+60|0;
        t=v+56|0;
        q=v+48|0;
        r=v+64|0;
        B=k;
        c[B>>2]=37;
        c[B+4>>2]=0;
        B=nh(k+1|0,9600,c[d+4>>2]|0)|0;
        c[m>>2]=n;
        a=Jg()|0;
        if(B)
        {
            c[g>>2]=c[d+8>>2];
            h[g+8>>3]=f;
            a=Am(n,30,a,k,g)|0
        }
        else
        {
            h[j>>3]=f;
            a=Am(n,30,a,k,j)|0
        }
        if((a|0)>29)
        {
            g=Jg()|0;
            c[l>>2]=c[d+8>>2];
            h[l+8>>3]=f;
            g=Bm(m,g,k,l)|0;
            a=c[m>>2]|0;
            if(!a)sd();
            else
            {
                w=a;
                z=a;
                s=g
            }
        }
        else
        {
            w=c[m>>2]|0;
            z=0;
            s=a
        }
        g=w+s|0;
        j=hh(w,g,d)|0;
        if((w|0)!=(n|0))
        {
            a=$d(s<<1)|0;
            if(!a)sd();
            else
            {
                x=w;
                y=a;
                A=a
            }
        }
        else
        {
            x=n;
            y=0;
            A=o
        }
        B=ff(d)|0;
        c[q>>2]=B;
        oh(x,j,g,A,u,t,q);
        Sn(B)|0;
        c[r>>2]=c[b>>2];
        b=c[u>>2]|0;
        B=c[t>>2]|0;
        c[p>>2]=c[r>>2];
        B=kc(p,A,b,B,d,e)|0;
        ae(y);
        ae(z);
        i=v;
        return B|0
    }
    function nh(b,c,d)
    {
        b=b|0;
        c=c|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,i=0;
        if(d&2048)
        {
            a[b>>0]=43;
            b=b+1|0
        }
        if(d&1024)
        {
            a[b>>0]=35;
            b=b+1|0
        }
        h=d&260;
        f=d>>>14;
        i=(h|0)==260;
        if(i)g=0;
        else
        {
            a[b>>0]=46;
            a[b+1>>0]=42;
            b=b+2|0;
            g=1
        }
        e=a[c>>0]|0;
        if(e<<24>>24)while(1)
            {
            c=c+1|0;
            d=b+1|0;
            a[b>>0]=e;
            e=a[c>>0]|0;
            if(!(e<<24>>24))
            {
                b=d;
                break
            }
            else b=d
        }
        do if((h|0)==256)if(!(f&1))
        {
            a[b>>0]=101;
            break
        }
        else
        {
            a[b>>0]=69;
            break
        }
        else if((h|0)==4)if(!(f&1))
        {
            a[b>>0]=102;
            break
        }
        else
        {
            a[b>>0]=70;
            break
        }
        else
        {
            d=(f&1|0)!=0;
            if(i)if(d)
            {
                a[b>>0]=65;
                break
            }
            else
            {
                a[b>>0]=97;
                break
            }
            else if(d)
            {
                a[b>>0]=71;
                break
            }
            else
            {
                a[b>>0]=103;
                break
            }
        }
        while(0);
        return g|0
    }
    function oh(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0;
        x=i;
        i=i+16|0;
        w=x;
        v=tk(j,6584)|0;
        t=tk(j,6728)|0;
        Nb[c[(c[t>>2]|0)+20>>2]&63](w,t);
        c[h>>2]=f;
        j=a[b>>0]|0;
        if(j<<24>>24==43|j<<24>>24==45)
        {
            u=Wb[c[(c[v>>2]|0)+28>>2]&15](v,j)|0;
            n=c[h>>2]|0;
            c[h>>2]=n+1;
            a[n>>0]=u;
            n=b+1|0
        }
        else n=b;
        u=e;
        a:do if(((u-n|0)>1?(a[n>>0]|0)==48:0)?(l=n+1|0,s=a[l>>0]|0,s<<24>>24==88|s<<24>>24==120):0)
        {
            s=Wb[c[(c[v>>2]|0)+28>>2]&15](v,48)|0;
            p=c[h>>2]|0;
            c[h>>2]=p+1;
            a[p>>0]=s;
            n=n+2|0;
            p=Wb[c[(c[v>>2]|0)+28>>2]&15](v,a[l>>0]|0)|0;
            s=c[h>>2]|0;
            c[h>>2]=s+1;
            a[s>>0]=p;
            if(n>>>0<e>>>0)
            {
                j=n;
                while(1)
                {
                    s=a[j>>0]|0;
                    if(!(oc(s,Jg()|0)|0))
                    {
                        s=n;
                        break a
                    }
                    j=j+1|0;
                    if(j>>>0>=e>>>0)
                    {
                        s=n;
                        break
                    }
                }
            }
            else
            {
                s=n;
                j=n
            }
        }
        else q=4;
        while(0);
        b:do if((q|0)==4)if(n>>>0<e>>>0)
        {
            j=n;
            while(1)
            {
                s=a[j>>0]|0;
                if(!(nc(s,Jg()|0)|0))
                {
                    s=n;
                    break b
                }
                j=j+1|0;
                if(j>>>0>=e>>>0)
                {
                    s=n;
                    break
                }
            }
        }
        else
        {
            s=n;
            j=n
        }
        while(0);
        p=a[w>>0]|0;
        r=w+4|0;
        if(((p&1)==0?(p&255)>>>1:c[r>>2]|0)|0)
        {
            if((s|0)!=(j|0)?(m=j+-1|0,s>>>0<m>>>0):0)
            {
                l=s;
                do
                {
                    p=a[l>>0]|0;
                    a[l>>0]=a[m>>0]|0;
                    a[m>>0]=p;
                    l=l+1|0;
                    m=m+-1|0
                }
                while(l>>>0<m>>>0)
            }
            n=Qb[c[(c[t>>2]|0)+16>>2]&63](t)|0;
            o=w+8|0;
            p=w+1|0;
            if(s>>>0<j>>>0)
            {
                m=0;
                l=0;
                q=s;
                while(1)
                {
                    y=a[((a[w>>0]&1)==0?p:c[o>>2]|0)+l>>0]|0;
                    if(y<<24>>24>0&(m|0)==(y<<24>>24|0))
                    {
                        y=c[h>>2]|0;
                        c[h>>2]=y+1;
                        a[y>>0]=n;
                        y=a[w>>0]|0;
                        m=0;
                        l=(l>>>0<(((y&1)==0?(y&255)>>>1:c[r>>2]|0)+-1|0)>>>0&1)+l|0
                    }
                    z=Wb[c[(c[v>>2]|0)+28>>2]&15](v,a[q>>0]|0)|0;
                    y=c[h>>2]|0;
                    c[h>>2]=y+1;
                    a[y>>0]=z;
                    q=q+1|0;
                    if(q>>>0>=j>>>0)break;
                    else m=m+1|0
                }
            }
            l=f+(s-b)|0;
            m=c[h>>2]|0;
            if((l|0)!=(m|0)?(k=m+-1|0,l>>>0<k>>>0):0)
            {
                do
                {
                    z=a[l>>0]|0;
                    a[l>>0]=a[k>>0]|0;
                    a[k>>0]=z;
                    l=l+1|0;
                    k=k+-1|0
                }
                while(l>>>0<k>>>0);
                m=v
            }
            else m=v
        }
        else
        {
            Ub[c[(c[v>>2]|0)+32>>2]&7](v,s,j,c[h>>2]|0)|0;
            c[h>>2]=(c[h>>2]|0)+(j-s);
            m=v
        }
        c:do if(j>>>0<e>>>0)
        {
            while(1)
            {
                k=a[j>>0]|0;
                if(k<<24>>24==46)break;
                y=Wb[c[(c[m>>2]|0)+28>>2]&15](v,k)|0;
                z=c[h>>2]|0;
                c[h>>2]=z+1;
                a[z>>0]=y;
                j=j+1|0;
                if(j>>>0>=e>>>0)break c
            }
            y=Qb[c[(c[t>>2]|0)+12>>2]&63](t)|0;
            z=c[h>>2]|0;
            c[h>>2]=z+1;
            a[z>>0]=y;
            j=j+1|0
        }
        while(0);
        Ub[c[(c[v>>2]|0)+32>>2]&7](v,j,e,c[h>>2]|0)|0;
        z=(c[h>>2]|0)+(u-j)|0;
        c[h>>2]=z;
        c[g>>2]=(d|0)==(e|0)?z:f+(d-b)|0;
        Ke(w);
        i=x;
        return
    }
    function ph(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=+f;
        var g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0;
        x=i;
        i=i+176|0;
        r=x+56|0;
        n=x+48|0;
        m=x+8|0;
        j=x+24|0;
        g=x+32|0;
        l=x;
        p=x+80|0;
        o=x+60|0;
        q=x+110|0;
        w=x+68|0;
        v=x+72|0;
        s=x+64|0;
        t=x+76|0;
        k=l;
        c[k>>2]=37;
        c[k+4>>2]=0;
        k=nh(l+1|0,9608,c[d+4>>2]|0)|0;
        c[o>>2]=p;
        a=Jg()|0;
        if(k)
        {
            c[g>>2]=c[d+8>>2];
            h[g+8>>3]=f;
            a=Am(p,30,a,l,g)|0
        }
        else
        {
            h[j>>3]=f;
            a=Am(p,30,a,l,j)|0
        }
        if((a|0)>29)
        {
            a=Jg()|0;
            if(k)
            {
                c[m>>2]=c[d+8>>2];
                h[m+8>>3]=f;
                g=Bm(o,a,l,m)|0
            }
            else
            {
                h[n>>3]=f;
                g=Bm(o,a,l,n)|0
            }
            a=c[o>>2]|0;
            if(!a)sd();
            else
            {
                y=a;
                B=a;
                u=g
            }
        }
        else
        {
            y=c[o>>2]|0;
            B=0;
            u=a
        }
        g=y+u|0;
        j=hh(y,g,d)|0;
        if((y|0)!=(p|0))
        {
            a=$d(u<<1)|0;
            if(!a)sd();
            else
            {
                z=y;
                A=a;
                C=a
            }
        }
        else
        {
            z=p;
            A=0;
            C=q
        }
        y=ff(d)|0;
        c[s>>2]=y;
        oh(z,j,g,C,w,v,s);
        Sn(y)|0;
        c[t>>2]=c[b>>2];
        z=c[w>>2]|0;
        b=c[v>>2]|0;
        c[r>>2]=c[t>>2];
        b=kc(r,C,z,b,d,e)|0;
        ae(A);
        ae(B);
        i=x;
        return b|0
    }
    function qh(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        h=i;
        i=i+80|0;
        m=h;
        b=h+72|0;
        j=h+52|0;
        k=h+12|0;
        o=h+4|0;
        n=h+8|0;
        a[b>>0]=a[9616]|0;
        a[b+1>>0]=a[9617]|0;
        a[b+2>>0]=a[9618]|0;
        a[b+3>>0]=a[9619]|0;
        a[b+4>>0]=a[9620]|0;
        a[b+5>>0]=a[9621]|0;
        l=Jg()|0;
        c[m>>2]=g;
        b=Am(j,20,l,b,m)|0;
        l=j+b|0;
        g=hh(j,l,e)|0;
        p=ff(e)|0;
        c[o>>2]=p;
        o=tk(o,6584)|0;
        Sn(p)|0;
        Ub[c[(c[o>>2]|0)+32>>2]&7](o,j,l,k)|0;
        b=k+b|0;
        c[n>>2]=c[d>>2];
        c[m>>2]=c[n>>2];
        b=kc(m,k,(g|0)==(l|0)?b:k+(g-j)|0,b,e,f)|0;
        i=h;
        return b|0
    }
    function rh(a)
    {
        a=a|0;
        return
    }
    function sh(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function th(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        m=i;
        i=i+32|0;
        h=m+20|0;
        j=m;
        k=m+4|0;
        l=m+8|0;
        if(!(c[e+4>>2]&1))
        {
            k=c[(c[b>>2]|0)+24>>2]|0;
            c[j>>2]=c[d>>2];
            c[h>>2]=c[j>>2];
            h=Xb[k&31](b,h,e,f,g&1)|0
        }
        else
        {
            j=ff(e)|0;
            c[k>>2]=j;
            h=tk(k,6736)|0;
            Sn(j)|0;
            j=c[h>>2]|0;
            if(g)Nb[c[j+24>>2]&63](l,h);
            else Nb[c[j+28>>2]&63](l,h);
            e=a[l>>0]|0;
            k=(e&1)==0;
            h=l+4|0;
            g=l+8|0;
            b=k?h:l+4|0;
            h=k?h:c[l+8>>2]|0;
            k=(e&1)==0;
            if((h|0)!=((k?b:c[g>>2]|0)+((k?(e&255)>>>1:c[b>>2]|0)<<2)|0))do
                {
                j=c[h>>2]|0;
                k=c[d>>2]|0;
                if(k)
                {
                    f=k+24|0;
                    e=c[f>>2]|0;
                    if((e|0)==(c[k+28>>2]|0))j=Wb[c[(c[k>>2]|0)+52>>2]&15](k,j)|0;
                    else
                    {
                        c[f>>2]=e+4;
                        c[e>>2]=j
                    }
                    if((j|0)==-1)c[d>>2]=0
                }
                h=h+4|0;
                e=a[l>>0]|0;
                k=(e&1)==0
            }
            while((h|0)!=((k?b:c[g>>2]|0)+((k?(e&255)>>>1:c[b>>2]|0)<<2)|0));
            h=c[d>>2]|0;
            Ve(l)
        }
        i=m;
        return h|0
    }
    function uh(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;
        h=i;
        i=i+128|0;
        k=h;
        o=h+116|0;
        q=h+104|0;
        j=h+8|0;
        m=h+92|0;
        b=h+96|0;
        n=h+4|0;
        l=h+100|0;
        a[o>>0]=a[9576]|0;
        a[o+1>>0]=a[9577]|0;
        a[o+2>>0]=a[9578]|0;
        a[o+3>>0]=a[9579]|0;
        a[o+4>>0]=a[9580]|0;
        a[o+5>>0]=a[9581]|0;
        gh(o+1|0,9584,1,c[e+4>>2]|0);
        p=Jg()|0;
        c[k>>2]=g;
        o=q+(Am(q,12,p,o,k)|0)|0;
        p=hh(q,o,e)|0;
        g=ff(e)|0;
        c[n>>2]=g;
        vh(q,p,o,j,m,b,n);
        Sn(g)|0;
        c[l>>2]=c[d>>2];
        d=c[m>>2]|0;
        b=c[b>>2]|0;
        c[k>>2]=c[l>>2];
        b=Cm(k,j,d,b,e,f)|0;
        i=h;
        return b|0
    }
    function vh(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;
        t=i;
        i=i+16|0;
        s=t;
        r=tk(j,6576)|0;
        k=tk(j,6736)|0;
        Nb[c[(c[k>>2]|0)+20>>2]&63](s,k);
        o=a[s>>0]|0;
        q=s+4|0;
        if(((o&1)==0?(o&255)>>>1:c[q>>2]|0)|0)
        {
            c[h>>2]=f;
            j=a[b>>0]|0;
            if(j<<24>>24==43|j<<24>>24==45)
            {
                o=Wb[c[(c[r>>2]|0)+44>>2]&15](r,j)|0;
                j=c[h>>2]|0;
                c[h>>2]=j+4;
                c[j>>2]=o;
                j=b+1|0
            }
            else j=b;
            if(((e-j|0)>1?(a[j>>0]|0)==48:0)?(l=j+1|0,o=a[l>>0]|0,o<<24>>24==88|o<<24>>24==120):0)
            {
                p=Wb[c[(c[r>>2]|0)+44>>2]&15](r,48)|0;
                o=c[h>>2]|0;
                c[h>>2]=o+4;
                c[o>>2]=p;
                o=Wb[c[(c[r>>2]|0)+44>>2]&15](r,a[l>>0]|0)|0;
                p=c[h>>2]|0;
                c[h>>2]=p+4;
                c[p>>2]=o;
                p=j+2|0
            }
            else p=j;
            if((p|0)!=(e|0)?(m=e+-1|0,p>>>0<m>>>0):0)
            {
                l=p;
                j=m;
                do
                {
                    o=a[l>>0]|0;
                    a[l>>0]=a[j>>0]|0;
                    a[j>>0]=o;
                    l=l+1|0;
                    j=j+-1|0
                }
                while(l>>>0<j>>>0)
            }
            k=Qb[c[(c[k>>2]|0)+16>>2]&63](k)|0;
            m=s+8|0;
            n=s+1|0;
            if(p>>>0<e>>>0)
            {
                j=0;
                l=0;
                o=p;
                while(1)
                {
                    u=a[((a[s>>0]&1)==0?n:c[m>>2]|0)+l>>0]|0;
                    if(u<<24>>24!=0&(j|0)==(u<<24>>24|0))
                    {
                        u=c[h>>2]|0;
                        c[h>>2]=u+4;
                        c[u>>2]=k;
                        u=a[s>>0]|0;
                        j=0;
                        l=(l>>>0<(((u&1)==0?(u&255)>>>1:c[q>>2]|0)+-1|0)>>>0&1)+l|0
                    }
                    v=Wb[c[(c[r>>2]|0)+44>>2]&15](r,a[o>>0]|0)|0;
                    u=c[h>>2]|0;
                    c[h>>2]=u+4;
                    c[u>>2]=v;
                    o=o+1|0;
                    if(o>>>0>=e>>>0)break;
                    else j=j+1|0
                }
            }
            l=f+(p-b<<2)|0;
            j=c[h>>2]|0;
            if((l|0)!=(j|0))
            {
                k=j+-4|0;
                if(l>>>0<k>>>0)do
                    {
                    v=c[l>>2]|0;
                    c[l>>2]=c[k>>2];
                    c[k>>2]=v;
                    l=l+4|0;
                    k=k+-4|0
                }
                while(l>>>0<k>>>0)
            }
            else j=l
        }
        else
        {
            Ub[c[(c[r>>2]|0)+48>>2]&7](r,b,e,f)|0;
            j=f+(e-b<<2)|0;
            c[h>>2]=j
        }
        c[g>>2]=(d|0)==(e|0)?j:f+(d-b<<2)|0;
        Ke(s);
        i=t;
        return
    }
    function wh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;
        h=i;
        i=i+224|0;
        k=h+8|0;
        o=h;
        p=h+196|0;
        j=h+16|0;
        m=h+180|0;
        a=h+184|0;
        n=h+188|0;
        l=h+192|0;
        q=o;
        c[q>>2]=37;
        c[q+4>>2]=0;
        gh(o+1|0,9592,1,c[d+4>>2]|0);
        q=Jg()|0;
        r=k;
        c[r>>2]=f;
        c[r+4>>2]=g;
        f=p+(Am(p,22,q,o,k)|0)|0;
        o=hh(p,f,d)|0;
        g=ff(d)|0;
        c[n>>2]=g;
        vh(p,o,f,j,m,a,n);
        Sn(g)|0;
        c[l>>2]=c[b>>2];
        b=c[m>>2]|0;
        a=c[a>>2]|0;
        c[k>>2]=c[l>>2];
        a=Cm(k,j,b,a,d,e)|0;
        i=h;
        return a|0
    }
    function xh(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;
        h=i;
        i=i+128|0;
        k=h;
        o=h+116|0;
        q=h+104|0;
        j=h+8|0;
        m=h+92|0;
        b=h+96|0;
        n=h+4|0;
        l=h+100|0;
        a[o>>0]=a[9576]|0;
        a[o+1>>0]=a[9577]|0;
        a[o+2>>0]=a[9578]|0;
        a[o+3>>0]=a[9579]|0;
        a[o+4>>0]=a[9580]|0;
        a[o+5>>0]=a[9581]|0;
        gh(o+1|0,9584,0,c[e+4>>2]|0);
        p=Jg()|0;
        c[k>>2]=g;
        o=q+(Am(q,12,p,o,k)|0)|0;
        p=hh(q,o,e)|0;
        g=ff(e)|0;
        c[n>>2]=g;
        vh(q,p,o,j,m,b,n);
        Sn(g)|0;
        c[l>>2]=c[d>>2];
        d=c[m>>2]|0;
        b=c[b>>2]|0;
        c[k>>2]=c[l>>2];
        b=Cm(k,j,d,b,e,f)|0;
        i=h;
        return b|0
    }
    function yh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;
        h=i;
        i=i+240|0;
        k=h+8|0;
        o=h;
        p=h+204|0;
        j=h+16|0;
        m=h+188|0;
        a=h+192|0;
        n=h+196|0;
        l=h+200|0;
        q=o;
        c[q>>2]=37;
        c[q+4>>2]=0;
        gh(o+1|0,9592,0,c[d+4>>2]|0);
        q=Jg()|0;
        r=k;
        c[r>>2]=f;
        c[r+4>>2]=g;
        f=p+(Am(p,23,q,o,k)|0)|0;
        o=hh(p,f,d)|0;
        g=ff(d)|0;
        c[n>>2]=g;
        vh(p,o,f,j,m,a,n);
        Sn(g)|0;
        c[l>>2]=c[b>>2];
        b=c[m>>2]|0;
        a=c[a>>2]|0;
        c[k>>2]=c[l>>2];
        a=Cm(k,j,b,a,d,e)|0;
        i=h;
        return a|0
    }
    function zh(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=+f;
        var g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0;
        y=i;
        i=i+336|0;
        p=y+296|0;
        l=y;
        j=y+16|0;
        g=y+24|0;
        k=y+40|0;
        n=y+300|0;
        m=y+52|0;
        o=y+56|0;
        u=y+284|0;
        t=y+288|0;
        q=y+48|0;
        r=y+292|0;
        B=k;
        c[B>>2]=37;
        c[B+4>>2]=0;
        B=nh(k+1|0,9600,c[d+4>>2]|0)|0;
        c[m>>2]=n;
        a=Jg()|0;
        if(B)
        {
            c[g>>2]=c[d+8>>2];
            h[g+8>>3]=f;
            a=Am(n,30,a,k,g)|0
        }
        else
        {
            h[j>>3]=f;
            a=Am(n,30,a,k,j)|0
        }
        if((a|0)>29)
        {
            g=Jg()|0;
            c[l>>2]=c[d+8>>2];
            h[l+8>>3]=f;
            g=Bm(m,g,k,l)|0;
            a=c[m>>2]|0;
            if(!a)sd();
            else
            {
                v=a;
                A=a;
                s=g
            }
        }
        else
        {
            v=c[m>>2]|0;
            A=0;
            s=a
        }
        g=v+s|0;
        j=hh(v,g,d)|0;
        if((v|0)!=(n|0))
        {
            a=$d(s<<3)|0;
            if(!a)sd();
            else
            {
                w=v;
                z=a;
                x=a
            }
        }
        else
        {
            w=n;
            z=0;
            x=o
        }
        B=ff(d)|0;
        c[q>>2]=B;
        Ah(w,j,g,x,u,t,q);
        Sn(B)|0;
        c[r>>2]=c[b>>2];
        B=c[u>>2]|0;
        a=c[t>>2]|0;
        c[p>>2]=c[r>>2];
        a=Cm(p,x,B,a,d,e)|0;
        c[b>>2]=a;
        if(z)ae(z);
        ae(A);
        i=y;
        return a|0
    }
    function Ah(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0;
        w=i;
        i=i+16|0;
        v=w;
        u=tk(j,6576)|0;
        s=tk(j,6736)|0;
        Nb[c[(c[s>>2]|0)+20>>2]&63](v,s);
        c[h>>2]=f;
        j=a[b>>0]|0;
        if(j<<24>>24==43|j<<24>>24==45)
        {
            t=Wb[c[(c[u>>2]|0)+44>>2]&15](u,j)|0;
            l=c[h>>2]|0;
            c[h>>2]=l+4;
            c[l>>2]=t;
            l=b+1|0
        }
        else l=b;
        t=e;
        a:do if(((t-l|0)>1?(a[l>>0]|0)==48:0)?(k=l+1|0,p=a[k>>0]|0,p<<24>>24==88|p<<24>>24==120):0)
        {
            p=Wb[c[(c[u>>2]|0)+44>>2]&15](u,48)|0;
            n=c[h>>2]|0;
            c[h>>2]=n+4;
            c[n>>2]=p;
            l=l+2|0;
            n=Wb[c[(c[u>>2]|0)+44>>2]&15](u,a[k>>0]|0)|0;
            p=c[h>>2]|0;
            c[h>>2]=p+4;
            c[p>>2]=n;
            if(l>>>0<e>>>0)
            {
                j=l;
                while(1)
                {
                    p=a[j>>0]|0;
                    if(!(oc(p,Jg()|0)|0))
                    {
                        r=l;
                        break a
                    }
                    j=j+1|0;
                    if(j>>>0>=e>>>0)
                    {
                        r=l;
                        break
                    }
                }
            }
            else
            {
                r=l;
                j=l
            }
        }
        else o=4;
        while(0);
        b:do if((o|0)==4)if(l>>>0<e>>>0)
        {
            j=l;
            while(1)
            {
                p=a[j>>0]|0;
                if(!(nc(p,Jg()|0)|0))
                {
                    r=l;
                    break b
                }
                j=j+1|0;
                if(j>>>0>=e>>>0)
                {
                    r=l;
                    break
                }
            }
        }
        else
        {
            r=l;
            j=l
        }
        while(0);
        p=a[v>>0]|0;
        q=v+4|0;
        if(((p&1)==0?(p&255)>>>1:c[q>>2]|0)|0)
        {
            if((r|0)!=(j|0)?(m=j+-1|0,r>>>0<m>>>0):0)
            {
                k=r;
                do
                {
                    p=a[k>>0]|0;
                    a[k>>0]=a[m>>0]|0;
                    a[m>>0]=p;
                    k=k+1|0;
                    m=m+-1|0
                }
                while(k>>>0<m>>>0)
            }
            l=Qb[c[(c[s>>2]|0)+16>>2]&63](s)|0;
            n=v+8|0;
            o=v+1|0;
            if(r>>>0<j>>>0)
            {
                m=0;
                k=0;
                p=r;
                while(1)
                {
                    x=a[((a[v>>0]&1)==0?o:c[n>>2]|0)+k>>0]|0;
                    if(x<<24>>24>0&(m|0)==(x<<24>>24|0))
                    {
                        x=c[h>>2]|0;
                        c[h>>2]=x+4;
                        c[x>>2]=l;
                        x=a[v>>0]|0;
                        m=0;
                        k=(k>>>0<(((x&1)==0?(x&255)>>>1:c[q>>2]|0)+-1|0)>>>0&1)+k|0
                    }
                    y=Wb[c[(c[u>>2]|0)+44>>2]&15](u,a[p>>0]|0)|0;
                    x=c[h>>2]|0;
                    c[h>>2]=x+4;
                    c[x>>2]=y;
                    p=p+1|0;
                    if(p>>>0>=j>>>0)break;
                    else m=m+1|0
                }
            }
            m=f+(r-b<<2)|0;
            k=c[h>>2]|0;
            if((m|0)!=(k|0))
            {
                l=k+-4|0;
                if(m>>>0<l>>>0)
                {
                    do
                    {
                        y=c[m>>2]|0;
                        c[m>>2]=c[l>>2];
                        c[l>>2]=y;
                        m=m+4|0;
                        l=l+-4|0
                    }
                    while(m>>>0<l>>>0);
                    l=u
                }
                else l=u
            }
            else
            {
                l=u;
                k=m
            }
        }
        else
        {
            Ub[c[(c[u>>2]|0)+48>>2]&7](u,r,j,c[h>>2]|0)|0;
            k=(c[h>>2]|0)+(j-r<<2)|0;
            c[h>>2]=k;
            l=u
        }
        c:do if(j>>>0<e>>>0)
        {
            while(1)
            {
                k=a[j>>0]|0;
                if(k<<24>>24==46)break;
                x=Wb[c[(c[l>>2]|0)+44>>2]&15](u,k)|0;
                y=c[h>>2]|0;
                k=y+4|0;
                c[h>>2]=k;
                c[y>>2]=x;
                j=j+1|0;
                if(j>>>0>=e>>>0)break c
            }
            x=Qb[c[(c[s>>2]|0)+12>>2]&63](s)|0;
            y=c[h>>2]|0;
            k=y+4|0;
            c[h>>2]=k;
            c[y>>2]=x;
            j=j+1|0
        }
        while(0);
        Ub[c[(c[u>>2]|0)+48>>2]&7](u,j,e,k)|0;
        y=(c[h>>2]|0)+(t-j<<2)|0;
        c[h>>2]=y;
        c[g>>2]=(d|0)==(e|0)?y:f+(d-b<<2)|0;
        Ke(v);
        i=w;
        return
    }
    function Bh(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=+f;
        var g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0;
        A=i;
        i=i+352|0;
        r=A+56|0;
        n=A+48|0;
        m=A+8|0;
        j=A+24|0;
        g=A+32|0;
        l=A;
        p=A+308|0;
        o=A+60|0;
        q=A+64|0;
        w=A+292|0;
        v=A+296|0;
        s=A+300|0;
        t=A+304|0;
        k=l;
        c[k>>2]=37;
        c[k+4>>2]=0;
        k=nh(l+1|0,9608,c[d+4>>2]|0)|0;
        c[o>>2]=p;
        a=Jg()|0;
        if(k)
        {
            c[g>>2]=c[d+8>>2];
            h[g+8>>3]=f;
            a=Am(p,30,a,l,g)|0
        }
        else
        {
            h[j>>3]=f;
            a=Am(p,30,a,l,j)|0
        }
        if((a|0)>29)
        {
            a=Jg()|0;
            if(k)
            {
                c[m>>2]=c[d+8>>2];
                h[m+8>>3]=f;
                g=Bm(o,a,l,m)|0
            }
            else
            {
                h[n>>3]=f;
                g=Bm(o,a,l,n)|0
            }
            a=c[o>>2]|0;
            if(!a)sd();
            else
            {
                x=a;
                C=a;
                u=g
            }
        }
        else
        {
            x=c[o>>2]|0;
            C=0;
            u=a
        }
        g=x+u|0;
        j=hh(x,g,d)|0;
        if((x|0)!=(p|0))
        {
            a=$d(u<<3)|0;
            if(!a)sd();
            else
            {
                y=x;
                B=a;
                z=a
            }
        }
        else
        {
            y=p;
            B=0;
            z=q
        }
        a=ff(d)|0;
        c[s>>2]=a;
        Ah(y,j,g,z,w,v,s);
        Sn(a)|0;
        c[t>>2]=c[b>>2];
        y=c[w>>2]|0;
        a=c[v>>2]|0;
        c[r>>2]=c[t>>2];
        a=Cm(r,z,y,a,d,e)|0;
        c[b>>2]=a;
        if(B)ae(B);
        ae(C);
        i=A;
        return a|0
    }
    function Ch(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        h=i;
        i=i+192|0;
        m=h;
        b=h+180|0;
        j=h+160|0;
        k=h+8|0;
        o=h+4|0;
        n=h+156|0;
        a[b>>0]=a[9616]|0;
        a[b+1>>0]=a[9617]|0;
        a[b+2>>0]=a[9618]|0;
        a[b+3>>0]=a[9619]|0;
        a[b+4>>0]=a[9620]|0;
        a[b+5>>0]=a[9621]|0;
        l=Jg()|0;
        c[m>>2]=g;
        b=Am(j,20,l,b,m)|0;
        l=j+b|0;
        g=hh(j,l,e)|0;
        p=ff(e)|0;
        c[o>>2]=p;
        o=tk(o,6576)|0;
        Sn(p)|0;
        Ub[c[(c[o>>2]|0)+48>>2]&7](o,j,l,k)|0;
        b=k+(b<<2)|0;
        c[n>>2]=c[d>>2];
        c[m>>2]=c[n>>2];
        b=Cm(m,k,(g|0)==(l|0)?b:k+(g-j<<2)|0,b,e,f)|0;
        i=h;
        return b|0
    }
    function Dh(e,f,g,h,j,k,l,m)
    {
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0;
        B=i;
        i=i+32|0;
        u=B+16|0;
        t=B;
        x=B+4|0;
        v=B+8|0;
        w=B+12|0;
        y=ff(h)|0;
        c[x>>2]=y;
        x=tk(x,6584)|0;
        Sn(y)|0;
        c[j>>2]=0;
        y=x+8|0;
        n=c[f>>2]|0;
        a:do if((l|0)!=(m|0))
        {
            b:while(1)
                {
                o=n;
                if(n)
                {
                    if((c[n+12>>2]|0)==(c[n+16>>2]|0)?(Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0)==-1:0)
                    {
                        c[f>>2]=0;
                        n=0;
                        o=0
                    }
                }
                else n=0;
                r=(n|0)==0;
                q=c[g>>2]|0;
                p=q;
                do if(q)
                {
                    if((c[q+12>>2]|0)==(c[q+16>>2]|0)?(Qb[c[(c[q>>2]|0)+36>>2]&63](q)|0)==-1:0)
                    {
                        c[g>>2]=0;
                        p=0;
                        A=11;
                        break
                    }
                    if(!r)
                    {
                        A=12;
                        break b
                    }
                }
                else A=11;
                while(0);
                if((A|0)==11)
                {
                    A=0;
                    if(r)
                    {
                        A=12;
                        break
                    }
                    else q=0
                }
                c:do if((Jb[c[(c[x>>2]|0)+36>>2]&31](x,a[l>>0]|0,0)|0)<<24>>24==37)
                {
                    r=l+1|0;
                    if((r|0)==(m|0))
                    {
                        A=15;
                        break b
                    }
                    s=Jb[c[(c[x>>2]|0)+36>>2]&31](x,a[r>>0]|0,0)|0;
                    if(s<<24>>24==48|s<<24>>24==69)
                    {
                        q=l+2|0;
                        if((q|0)==(m|0))
                        {
                            A=18;
                            break b
                        }
                        l=r;
                        q=Jb[c[(c[x>>2]|0)+36>>2]&31](x,a[q>>0]|0,0)|0;
                        n=s
                    }
                    else
                    {
                        q=s;
                        n=0
                    }
                    s=c[(c[e>>2]|0)+36>>2]|0;
                    c[v>>2]=o;
                    c[w>>2]=p;
                    c[t>>2]=c[v>>2];
                    c[u>>2]=c[w>>2];
                    c[f>>2]=Tb[s&15](e,t,u,h,j,k,q,n)|0;
                    l=l+2|0
                }
                else
                {
                    o=a[l>>0]|0;
                    if(o<<24>>24>-1?(z=c[y>>2]|0,(b[z+(o<<24>>24<<1)>>1]&8192)!=0):0)
                    {
                        do
                        {
                            l=l+1|0;
                            if((l|0)==(m|0))
                            {
                                l=m;
                                break
                            }
                            o=a[l>>0]|0;
                            if(o<<24>>24<=-1)break
                        }
                        while((b[z+(o<<24>>24<<1)>>1]&8192)!=0);
                        s=q;
                        r=q;
                        while(1)
                        {
                            if(n)
                            {
                                if((c[n+12>>2]|0)==(c[n+16>>2]|0)?(Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0)==-1:0)
                                {
                                    c[f>>2]=0;
                                    n=0
                                }
                            }
                            else n=0;
                            p=(n|0)==0;
                            do if(r)
                            {
                                if((c[r+12>>2]|0)!=(c[r+16>>2]|0))if(p)
                                {
                                    o=s;
                                    break
                                }
                                else break c;
                                if((Qb[c[(c[r>>2]|0)+36>>2]&63](r)|0)!=-1)if(p^(s|0)==0)
                                {
                                    o=s;
                                    r=s;
                                    break
                                }
                                else break c;
                                else
                                {
                                    c[g>>2]=0;
                                    o=0;
                                    A=37;
                                    break
                                }
                            }
                            else
                            {
                                o=s;
                                A=37
                            }
                            while(0);
                            if((A|0)==37)
                            {
                                A=0;
                                if(p)break c;
                                else r=0
                            }
                            q=n+12|0;
                            p=c[q>>2]|0;
                            s=n+16|0;
                            if((p|0)==(c[s>>2]|0))p=Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0;
                            else p=d[p>>0]|0;
                            if((p&255)<<24>>24<=-1)break c;
                            if(!(b[(c[y>>2]|0)+(p<<24>>24<<1)>>1]&8192))break c;
                            p=c[q>>2]|0;
                            if((p|0)==(c[s>>2]|0))
                            {
                                Qb[c[(c[n>>2]|0)+40>>2]&63](n)|0;
                                s=o;
                                continue
                            }
                            else
                            {
                                c[q>>2]=p+1;
                                s=o;
                                continue
                            }
                        }
                    }
                    p=n+12|0;
                    o=c[p>>2]|0;
                    q=n+16|0;
                    if((o|0)==(c[q>>2]|0))o=Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0;
                    else o=d[o>>0]|0;
                    s=Wb[c[(c[x>>2]|0)+12>>2]&15](x,o&255)|0;
                    if(s<<24>>24!=(Wb[c[(c[x>>2]|0)+12>>2]&15](x,a[l>>0]|0)|0)<<24>>24)
                    {
                        A=55;
                        break b
                    }
                    o=c[p>>2]|0;
                    if((o|0)==(c[q>>2]|0))Qb[c[(c[n>>2]|0)+40>>2]&63](n)|0;
                    else c[p>>2]=o+1;
                    l=l+1|0
                }
                while(0);
                n=c[f>>2]|0;
                if(!((l|0)!=(m|0)&(c[j>>2]|0)==0))break a
            }
            if((A|0)==12)
            {
                c[j>>2]=4;
                break
            }
            else if((A|0)==15)
            {
                c[j>>2]=4;
                break
            }
            else if((A|0)==18)
            {
                c[j>>2]=4;
                break
            }
            else if((A|0)==55)
            {
                c[j>>2]=4;
                n=c[f>>2]|0;
                break
            }
        }
        while(0);
        if(n)
        {
            if((c[n+12>>2]|0)==(c[n+16>>2]|0)?(Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0)==-1:0)
            {
                c[f>>2]=0;
                n=0
            }
        }
        else n=0;
        l=(n|0)==0;
        o=c[g>>2]|0;
        do if(o)
        {
            if((c[o+12>>2]|0)==(c[o+16>>2]|0)?(Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0)==-1:0)
            {
                c[g>>2]=0;
                A=65;
                break
            }
            if(!l)A=66
        }
        else A=65;
        while(0);
        if((A|0)==65?l:0)A=66;
        if((A|0)==66)c[j>>2]=c[j>>2]|2;
        i=B;
        return n|0
    }
    function Eh(a)
    {
        a=a|0;
        return
    }
    function Fh(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Gh(a)
    {
        a=a|0;
        return 2
    }
    function Hh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=Dh(a,k,j,e,f,g,9624,9632)|0;
        i=h;
        return a|0
    }
    function Ih(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        j=i;
        i=i+16|0;
        k=j+12|0;
        l=j;
        n=j+4|0;
        m=j+8|0;
        o=b+8|0;
        o=Qb[c[(c[o>>2]|0)+20>>2]&63](o)|0;
        c[n>>2]=c[d>>2];
        c[m>>2]=c[e>>2];
        e=a[o>>0]|0;
        p=(e&1)==0;
        d=p?o+1|0:c[o+8>>2]|0;
        e=d+(p?(e&255)>>>1:c[o+4>>2]|0)|0;
        c[l>>2]=c[n>>2];
        c[k>>2]=c[m>>2];
        b=Dh(b,l,k,f,g,h,d,e)|0;
        i=j;
        return b|0
    }
    function Jh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+8|0;
        m=h;
        k=h+4|0;
        l=ff(e)|0;
        c[m>>2]=l;
        e=tk(m,6584)|0;
        Sn(l)|0;
        c[k>>2]=c[d>>2];
        c[j>>2]=c[k>>2];
        Kh(a,g+24|0,b,j,f,e);
        i=h;
        return c[b>>2]|0
    }
    function Kh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0;
        h=i;
        i=i+16|0;
        j=h+4|0;
        k=h;
        a=a+8|0;
        a=Qb[c[c[a>>2]>>2]&63](a)|0;
        c[k>>2]=c[e>>2];
        c[j>>2]=c[k>>2];
        d=(fm(d,j,a,a+168|0,g,f,0)|0)-a|0;
        if((d|0)<168)c[b>>2]=((d|0)/12|0|0)%7|0;
        i=h;
        return
    }
    function Lh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+8|0;
        m=h;
        k=h+4|0;
        l=ff(e)|0;
        c[m>>2]=l;
        e=tk(m,6584)|0;
        Sn(l)|0;
        c[k>>2]=c[d>>2];
        c[j>>2]=c[k>>2];
        Mh(a,g+16|0,b,j,f,e);
        i=h;
        return c[b>>2]|0
    }
    function Mh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0;
        h=i;
        i=i+16|0;
        j=h+4|0;
        k=h;
        a=a+8|0;
        a=Qb[c[(c[a>>2]|0)+4>>2]&63](a)|0;
        c[k>>2]=c[e>>2];
        c[j>>2]=c[k>>2];
        d=(fm(d,j,a,a+288|0,g,f,0)|0)-a|0;
        if((d|0)<288)c[b>>2]=((d|0)/12|0|0)%12|0;
        i=h;
        return
    }
    function Nh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+8|0;
        m=h;
        k=h+4|0;
        l=ff(e)|0;
        c[m>>2]=l;
        e=tk(m,6584)|0;
        Sn(l)|0;
        c[k>>2]=c[d>>2];
        c[j>>2]=c[k>>2];
        Oh(a,g+20|0,b,j,f,e);
        i=h;
        return c[b>>2]|0
    }
    function Oh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,4)|0;
        if(!(c[f>>2]&4))
        {
            if((a|0)<69)a=a+2e3|0;
            else a=(a+-69|0)>>>0<31?a+1900|0:a;
            c[b>>2]=a+-1900
        }
        i=h;
        return
    }
    function Ph(b,d,e,f,g,h,j,k)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0;
        S=i;
        i=i+144|0;
        l=S+132|0;
        k=S+128|0;
        L=S+124|0;
        w=S+120|0;
        H=S+116|0;
        M=S+112|0;
        N=S+108|0;
        O=S+104|0;
        P=S+100|0;
        Q=S+96|0;
        R=S+92|0;
        m=S+88|0;
        n=S+84|0;
        o=S+80|0;
        p=S+76|0;
        q=S+72|0;
        r=S+68|0;
        s=S+64|0;
        t=S+24|0;
        u=S;
        v=S+4|0;
        x=S+8|0;
        y=S+12|0;
        z=S+16|0;
        A=S+20|0;
        B=S+28|0;
        C=S+32|0;
        D=S+36|0;
        E=S+40|0;
        F=S+44|0;
        G=S+48|0;
        I=S+52|0;
        J=S+56|0;
        K=S+60|0;
        c[g>>2]=0;
        U=ff(f)|0;
        c[L>>2]=U;
        L=tk(L,6584)|0;
        Sn(U)|0;
        do switch(j<<24>>24|0)
            {
            case 77:
                {
                    c[r>>2]=c[e>>2];
                    c[l>>2]=c[r>>2];
                    Vh(b,h+4|0,d,l,g,L);
                    T=26;
                    break
                }
            case 84:
                {
                    c[A>>2]=c[d>>2];
                    c[B>>2]=c[e>>2];
                    c[k>>2]=c[A>>2];
                    c[l>>2]=c[B>>2];
                    c[d>>2]=Dh(b,k,l,f,g,h,9672,9680)|0;
                    T=26;
                    break
                }
            case 116:case 110:
                {
                    c[s>>2]=c[e>>2];
                    c[l>>2]=c[s>>2];
                    Wh(b,d,l,g,L);
                    T=26;
                    break
                }
            case 119:
                {
                    c[C>>2]=c[e>>2];
                    c[l>>2]=c[C>>2];
                    Zh(b,h+24|0,d,l,g,L);
                    T=26;
                    break
                }
            case 88:
                {
                    T=b+8|0;
                    T=Qb[c[(c[T>>2]|0)+24>>2]&63](T)|0;
                    c[F>>2]=c[d>>2];
                    c[G>>2]=c[e>>2];
                    e=a[T>>0]|0;
                    Q=(e&1)==0;
                    U=Q?T+1|0:c[T+8>>2]|0;
                    T=U+(Q?(e&255)>>>1:c[T+4>>2]|0)|0;
                    c[k>>2]=c[F>>2];
                    c[l>>2]=c[G>>2];
                    c[d>>2]=Dh(b,k,l,f,g,h,U,T)|0;
                    T=26;
                    break
                }
            case 121:
                {
                    c[I>>2]=c[e>>2];
                    c[l>>2]=c[I>>2];
                    Oh(b,h+20|0,d,l,g,L);
                    T=26;
                    break
                }
            case 120:
                {
                    U=c[(c[b>>2]|0)+20>>2]|0;
                    c[D>>2]=c[d>>2];
                    c[E>>2]=c[e>>2];
                    c[k>>2]=c[D>>2];
                    c[l>>2]=c[E>>2];
                    k=Ob[U&63](b,k,l,f,g,h)|0;
                    break
                }
            case 89:
                {
                    c[J>>2]=c[e>>2];
                    c[l>>2]=c[J>>2];
                    _h(b,h+20|0,d,l,g,L);
                    T=26;
                    break
                }
            case 101:case 100:
                {
                    c[O>>2]=c[e>>2];
                    c[l>>2]=c[O>>2];
                    Qh(b,h+12|0,d,l,g,L);
                    T=26;
                    break
                }
            case 112:
                {
                    c[t>>2]=c[e>>2];
                    c[l>>2]=c[t>>2];
                    Xh(b,h+8|0,d,l,g,L);
                    T=26;
                    break
                }
            case 82:
                {
                    c[x>>2]=c[d>>2];
                    c[y>>2]=c[e>>2];
                    c[k>>2]=c[x>>2];
                    c[l>>2]=c[y>>2];
                    c[d>>2]=Dh(b,k,l,f,g,h,9664,9669)|0;
                    T=26;
                    break
                }
            case 37:
                {
                    c[K>>2]=c[e>>2];
                    c[l>>2]=c[K>>2];
                    $h(b,d,l,g,L);
                    T=26;
                    break
                }
            case 104:case 66:case 98:
                {
                    c[H>>2]=c[e>>2];
                    c[l>>2]=c[H>>2];
                    Mh(b,h+16|0,d,l,g,L);
                    T=26;
                    break
                }
            case 72:
                {
                    c[n>>2]=c[e>>2];
                    c[l>>2]=c[n>>2];
                    Rh(b,h+8|0,d,l,g,L);
                    T=26;
                    break
                }
            case 99:
                {
                    T=b+8|0;
                    T=Qb[c[(c[T>>2]|0)+12>>2]&63](T)|0;
                    c[M>>2]=c[d>>2];
                    c[N>>2]=c[e>>2];
                    e=a[T>>0]|0;
                    Q=(e&1)==0;
                    U=Q?T+1|0:c[T+8>>2]|0;
                    T=U+(Q?(e&255)>>>1:c[T+4>>2]|0)|0;
                    c[k>>2]=c[M>>2];
                    c[l>>2]=c[N>>2];
                    c[d>>2]=Dh(b,k,l,f,g,h,U,T)|0;
                    T=26;
                    break
                }
            case 73:
                {
                    c[o>>2]=c[e>>2];
                    c[l>>2]=c[o>>2];
                    Sh(b,h+8|0,d,l,g,L);
                    T=26;
                    break
                }
            case 106:
                {
                    c[p>>2]=c[e>>2];
                    c[l>>2]=c[p>>2];
                    Th(b,h+28|0,d,l,g,L);
                    T=26;
                    break
                }
            case 114:
                {
                    c[u>>2]=c[d>>2];
                    c[v>>2]=c[e>>2];
                    c[k>>2]=c[u>>2];
                    c[l>>2]=c[v>>2];
                    c[d>>2]=Dh(b,k,l,f,g,h,9648,9659)|0;
                    T=26;
                    break
                }
            case 65:case 97:
                {
                    c[w>>2]=c[e>>2];
                    c[l>>2]=c[w>>2];
                    Kh(b,h+24|0,d,l,g,L);
                    T=26;
                    break
                }
            case 68:
                {
                    c[P>>2]=c[d>>2];
                    c[Q>>2]=c[e>>2];
                    c[k>>2]=c[P>>2];
                    c[l>>2]=c[Q>>2];
                    c[d>>2]=Dh(b,k,l,f,g,h,9632,9640)|0;
                    T=26;
                    break
                }
            case 70:
                {
                    c[R>>2]=c[d>>2];
                    c[m>>2]=c[e>>2];
                    c[k>>2]=c[R>>2];
                    c[l>>2]=c[m>>2];
                    c[d>>2]=Dh(b,k,l,f,g,h,9640,9648)|0;
                    T=26;
                    break
                }
            case 109:
                {
                    c[q>>2]=c[e>>2];
                    c[l>>2]=c[q>>2];
                    Uh(b,h+16|0,d,l,g,L);
                    T=26;
                    break
                }
            case 83:
                {
                    c[z>>2]=c[e>>2];
                    c[l>>2]=c[z>>2];
                    Yh(b,h,d,l,g,L);
                    T=26;
                    break
                }
            default:
                {
                    c[g>>2]=c[g>>2]|4;
                    T=26
                }
        }
        while(0);
        if((T|0)==26)k=c[d>>2]|0;
        i=S;
        return k|0
    }
    function Qh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a+-1|0)>>>0<31&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function Rh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a|0)<24&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function Sh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a+-1|0)>>>0<12&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function Th(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,3)|0;
        d=c[f>>2]|0;
        if((a|0)<366&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function Uh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a|0)<13&(d&4|0)==0)c[b>>2]=a+-1;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function Vh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a|0)<60&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function Wh(a,e,f,g,h)
    {
        a=a|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var i=0,j=0,k=0;
        j=h+8|0;
        a:while(1)
            {
            h=c[e>>2]|0;
            do if(h)
            {
                if((c[h+12>>2]|0)==(c[h+16>>2]|0))if((Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0)==-1)
                {
                    c[e>>2]=0;
                    h=0;
                    break
                }
                else
                {
                    h=c[e>>2]|0;
                    break
                }
            }
            else h=0;
            while(0);
            h=(h|0)==0;
            a=c[f>>2]|0;
            do if(a)
            {
                if((c[a+12>>2]|0)!=(c[a+16>>2]|0))if(h)break;
                else break a;
                if((Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0)!=-1)if(h)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    k=12;
                    break
                }
            }
            else k=12;
            while(0);
            if((k|0)==12)
            {
                k=0;
                if(h)
                {
                    a=0;
                    break
                }
                else a=0
            }
            h=c[e>>2]|0;
            i=c[h+12>>2]|0;
            if((i|0)==(c[h+16>>2]|0))h=Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0;
            else h=d[i>>0]|0;
            if((h&255)<<24>>24<=-1)break;
            if(!(b[(c[j>>2]|0)+(h<<24>>24<<1)>>1]&8192))break;
            h=c[e>>2]|0;
            a=h+12|0;
            i=c[a>>2]|0;
            if((i|0)==(c[h+16>>2]|0))
            {
                Qb[c[(c[h>>2]|0)+40>>2]&63](h)|0;
                continue
            }
            else
            {
                c[a>>2]=i+1;
                continue
            }
        }
        h=c[e>>2]|0;
        do if(h)
        {
            if((c[h+12>>2]|0)==(c[h+16>>2]|0))if((Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0)==-1)
            {
                c[e>>2]=0;
                h=0;
                break
            }
            else
            {
                h=c[e>>2]|0;
                break
            }
        }
        else h=0;
        while(0);
        h=(h|0)==0;
        do if(a)
        {
            if((c[a+12>>2]|0)==(c[a+16>>2]|0)?(Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0)==-1:0)
            {
                c[f>>2]=0;
                k=32;
                break
            }
            if(!h)k=33
        }
        else k=32;
        while(0);
        if((k|0)==32?h:0)k=33;
        if((k|0)==33)c[g>>2]=c[g>>2]|2;
        return
    }
    function Xh(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0;
        n=i;
        i=i+16|0;
        k=n+4|0;
        l=n;
        m=b+8|0;
        m=Qb[c[(c[m>>2]|0)+8>>2]&63](m)|0;
        b=a[m>>0]|0;
        if(!(b&1))j=(b&255)>>>1;
        else j=c[m+4>>2]|0;
        b=a[m+12>>0]|0;
        if(!(b&1))b=(b&255)>>>1;
        else b=c[m+16>>2]|0;
        do if((j|0)!=(0-b|0))
        {
            c[l>>2]=c[f>>2];
            c[k>>2]=c[l>>2];
            b=fm(e,k,m,m+24|0,h,g,0)|0;
            j=c[d>>2]|0;
            if((b|0)==(m|0)&(j|0)==12)
            {
                c[d>>2]=0;
                break
            }
            if((j|0)<12&(b-m|0)==12)c[d>>2]=j+12
        }
        else c[g>>2]=c[g>>2]|4;
        while(0);
        i=n;
        return
    }
    function Yh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a|0)<61&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function Zh(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,1)|0;
        d=c[f>>2]|0;
        if((a|0)<7&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function _h(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Dm(d,a,f,g,4)|0;
        if(!(c[f>>2]&4))c[b>>2]=a+-1900;
        i=h;
        return
    }
    function $h(a,b,e,f,g)
    {
        a=a|0;
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,i=0,j=0;
        a=c[b>>2]|0;
        do if(a)
        {
            if((c[a+12>>2]|0)==(c[a+16>>2]|0))if((Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0)==-1)
            {
                c[b>>2]=0;
                a=0;
                break
            }
            else
            {
                a=c[b>>2]|0;
                break
            }
        }
        else a=0;
        while(0);
        h=(a|0)==0;
        a=c[e>>2]|0;
        do if(a)
        {
            if((c[a+12>>2]|0)==(c[a+16>>2]|0)?(Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0)==-1:0)
            {
                c[e>>2]=0;
                j=11;
                break
            }
            if(h)
            {
                i=a;
                j=13
            }
            else j=12
        }
        else j=11;
        while(0);
        if((j|0)==11)if(h)j=12;
        else
        {
            i=0;
            j=13
        }
        a:do if((j|0)==12)c[f>>2]=c[f>>2]|6;
        else if((j|0)==13)
        {
            a=c[b>>2]|0;
            h=c[a+12>>2]|0;
            if((h|0)==(c[a+16>>2]|0))a=Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0;
            else a=d[h>>0]|0;
            if((Jb[c[(c[g>>2]|0)+36>>2]&31](g,a&255,0)|0)<<24>>24!=37)
            {
                c[f>>2]=c[f>>2]|4;
                break
            }
            a=c[b>>2]|0;
            h=a+12|0;
            g=c[h>>2]|0;
            if((g|0)==(c[a+16>>2]|0))
            {
                Qb[c[(c[a>>2]|0)+40>>2]&63](a)|0;
                a=c[b>>2]|0;
                if(!a)a=0;
                else j=21
            }
            else
            {
                c[h>>2]=g+1;
                j=21
            }
            do if((j|0)==21)if((c[a+12>>2]|0)==(c[a+16>>2]|0))if((Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0)==-1)
            {
                c[b>>2]=0;
                a=0;
                break
            }
            else
            {
                a=c[b>>2]|0;
                break
            }
            while(0);
            a=(a|0)==0;
            do if(i)
            {
                if((c[i+12>>2]|0)==(c[i+16>>2]|0)?(Qb[c[(c[i>>2]|0)+36>>2]&63](i)|0)==-1:0)
                {
                    c[e>>2]=0;
                    j=30;
                    break
                }
                if(a)break a
            }
            else j=30;
            while(0);
            if((j|0)==30?!a:0)break;
            c[f>>2]=c[f>>2]|2
        }
        while(0);
        return
    }
    function ai(a,b,d,e,f,g,h,j)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;
        x=i;
        i=i+32|0;
        s=x+16|0;
        r=x;
        v=x+4|0;
        t=x+8|0;
        u=x+12|0;
        k=ff(e)|0;
        c[v>>2]=k;
        v=tk(v,6576)|0;
        Sn(k)|0;
        c[f>>2]=0;
        k=c[b>>2]|0;
        a:do if((h|0)!=(j|0))
        {
            b:while(1)
                {
                m=k;
                if(k)
                {
                    l=c[k+12>>2]|0;
                    if((l|0)==(c[k+16>>2]|0))l=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                    else l=c[l>>2]|0;
                    if((l|0)==-1)
                    {
                        c[b>>2]=0;
                        k=0;
                        o=1;
                        q=0
                    }
                    else
                    {
                        o=0;
                        q=m
                    }
                }
                else
                {
                    k=0;
                    o=1;
                    q=m
                }
                n=c[d>>2]|0;
                l=n;
                do if(n)
                {
                    m=c[n+12>>2]|0;
                    if((m|0)==(c[n+16>>2]|0))m=Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0;
                    else m=c[m>>2]|0;
                    if((m|0)!=-1)if(o)break;
                    else
                    {
                        w=16;
                        break b
                    }
                    else
                    {
                        c[d>>2]=0;
                        l=0;
                        w=14;
                        break
                    }
                }
                else w=14;
                while(0);
                if((w|0)==14)
                {
                    w=0;
                    if(o)
                    {
                        w=16;
                        break
                    }
                    else n=0
                }
                c:do if((Jb[c[(c[v>>2]|0)+52>>2]&31](v,c[h>>2]|0,0)|0)<<24>>24==37)
                {
                    m=h+4|0;
                    if((m|0)==(j|0))
                    {
                        w=19;
                        break b
                    }
                    o=Jb[c[(c[v>>2]|0)+52>>2]&31](v,c[m>>2]|0,0)|0;
                    if(o<<24>>24==48|o<<24>>24==69)
                    {
                        n=h+8|0;
                        if((n|0)==(j|0))
                        {
                            w=22;
                            break b
                        }
                        h=m;
                        m=Jb[c[(c[v>>2]|0)+52>>2]&31](v,c[n>>2]|0,0)|0;
                        k=o
                    }
                    else
                    {
                        m=o;
                        k=0
                    }
                    p=c[(c[a>>2]|0)+36>>2]|0;
                    c[t>>2]=q;
                    c[u>>2]=l;
                    c[r>>2]=c[t>>2];
                    c[s>>2]=c[u>>2];
                    c[b>>2]=Tb[p&15](a,r,s,e,f,g,m,k)|0;
                    h=h+8|0
                }
                else
                {
                    if(!(Jb[c[(c[v>>2]|0)+12>>2]&31](v,8192,c[h>>2]|0)|0))
                    {
                        m=k+12|0;
                        l=c[m>>2]|0;
                        n=k+16|0;
                        if((l|0)==(c[n>>2]|0))l=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                        else l=c[l>>2]|0;
                        p=Wb[c[(c[v>>2]|0)+28>>2]&15](v,l)|0;
                        if((p|0)!=(Wb[c[(c[v>>2]|0)+28>>2]&15](v,c[h>>2]|0)|0))
                        {
                            w=59;
                            break b
                        }
                        l=c[m>>2]|0;
                        if((l|0)==(c[n>>2]|0))Qb[c[(c[k>>2]|0)+40>>2]&63](k)|0;
                        else c[m>>2]=l+4;
                        h=h+4|0;
                        break
                    }
                    do
                    {
                        h=h+4|0;
                        if((h|0)==(j|0))
                        {
                            h=j;
                            break
                        }
                    }
                    while(Jb[c[(c[v>>2]|0)+12>>2]&31](v,8192,c[h>>2]|0)|0);
                    q=n;
                    o=n;
                    while(1)
                    {
                        if(k)
                        {
                            l=c[k+12>>2]|0;
                            if((l|0)==(c[k+16>>2]|0))l=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                            else l=c[l>>2]|0;
                            if((l|0)==-1)
                            {
                                c[b>>2]=0;
                                m=1;
                                k=0
                            }
                            else m=0
                        }
                        else
                        {
                            m=1;
                            k=0
                        }
                        do if(o)
                        {
                            l=c[o+12>>2]|0;
                            if((l|0)==(c[o+16>>2]|0))l=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                            else l=c[l>>2]|0;
                            if((l|0)!=-1)if(m^(q|0)==0)
                            {
                                l=q;
                                p=q;
                                break
                            }
                            else break c;
                            else
                            {
                                c[d>>2]=0;
                                l=0;
                                w=42;
                                break
                            }
                        }
                        else
                        {
                            l=q;
                            w=42
                        }
                        while(0);
                        if((w|0)==42)
                        {
                            w=0;
                            if(m)break c;
                            else p=0
                        }
                        n=k+12|0;
                        m=c[n>>2]|0;
                        o=k+16|0;
                        if((m|0)==(c[o>>2]|0))m=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                        else m=c[m>>2]|0;
                        if(!(Jb[c[(c[v>>2]|0)+12>>2]&31](v,8192,m)|0))break c;
                        m=c[n>>2]|0;
                        if((m|0)==(c[o>>2]|0))
                        {
                            Qb[c[(c[k>>2]|0)+40>>2]&63](k)|0;
                            q=l;
                            o=p;
                            continue
                        }
                        else
                        {
                            c[n>>2]=m+4;
                            q=l;
                            o=p;
                            continue
                        }
                    }
                }
                while(0);
                k=c[b>>2]|0;
                if(!((h|0)!=(j|0)&(c[f>>2]|0)==0))break a
            }
            if((w|0)==16)
            {
                c[f>>2]=4;
                break
            }
            else if((w|0)==19)
            {
                c[f>>2]=4;
                break
            }
            else if((w|0)==22)
            {
                c[f>>2]=4;
                break
            }
            else if((w|0)==59)
            {
                c[f>>2]=4;
                k=c[b>>2]|0;
                break
            }
        }
        while(0);
        if(k)
        {
            l=c[k+12>>2]|0;
            if((l|0)==(c[k+16>>2]|0))l=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else l=c[l>>2]|0;
            if((l|0)==-1)
            {
                c[b>>2]=0;
                k=0;
                h=1
            }
            else h=0
        }
        else
        {
            k=0;
            h=1
        }
        l=c[d>>2]|0;
        do if(l)
        {
            m=c[l+12>>2]|0;
            if((m|0)==(c[l+16>>2]|0))l=Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0;
            else l=c[m>>2]|0;
            if((l|0)!=-1)if(h)break;
            else
            {
                w=74;
                break
            }
            else
            {
                c[d>>2]=0;
                w=72;
                break
            }
        }
        else w=72;
        while(0);
        if((w|0)==72?h:0)w=74;
        if((w|0)==74)c[f>>2]=c[f>>2]|2;
        i=x;
        return k|0
    }
    function bi(a)
    {
        a=a|0;
        return
    }
    function ci(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function di(a)
    {
        a=a|0;
        return 2
    }
    function ei(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+12|0;
        k=h;
        m=h+4|0;
        l=h+8|0;
        c[m>>2]=c[b>>2];
        c[l>>2]=c[d>>2];
        c[k>>2]=c[m>>2];
        c[j>>2]=c[l>>2];
        a=ai(a,k,j,e,f,g,9680,9712)|0;
        i=h;
        return a|0
    }
    function fi(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;
        j=i;
        i=i+16|0;
        k=j+12|0;
        l=j;
        n=j+4|0;
        m=j+8|0;
        q=b+8|0;
        q=Qb[c[(c[q>>2]|0)+20>>2]&63](q)|0;
        c[n>>2]=c[d>>2];
        c[m>>2]=c[e>>2];
        o=a[q>>0]|0;
        p=(o&1)==0;
        e=q+4|0;
        d=p?e:c[q+8>>2]|0;
        e=d+((p?(o&255)>>>1:c[e>>2]|0)<<2)|0;
        c[l>>2]=c[n>>2];
        c[k>>2]=c[m>>2];
        b=ai(b,l,k,f,g,h,d,e)|0;
        i=j;
        return b|0
    }
    function gi(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+8|0;
        m=h;
        k=h+4|0;
        l=ff(e)|0;
        c[m>>2]=l;
        e=tk(m,6576)|0;
        Sn(l)|0;
        c[k>>2]=c[d>>2];
        c[j>>2]=c[k>>2];
        hi(a,g+24|0,b,j,f,e);
        i=h;
        return c[b>>2]|0
    }
    function hi(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0;
        h=i;
        i=i+16|0;
        j=h+4|0;
        k=h;
        a=a+8|0;
        a=Qb[c[c[a>>2]>>2]&63](a)|0;
        c[k>>2]=c[e>>2];
        c[j>>2]=c[k>>2];
        d=(qm(d,j,a,a+168|0,g,f,0)|0)-a|0;
        if((d|0)<168)c[b>>2]=((d|0)/12|0|0)%7|0;
        i=h;
        return
    }
    function ii(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+8|0;
        m=h;
        k=h+4|0;
        l=ff(e)|0;
        c[m>>2]=l;
        e=tk(m,6576)|0;
        Sn(l)|0;
        c[k>>2]=c[d>>2];
        c[j>>2]=c[k>>2];
        ji(a,g+16|0,b,j,f,e);
        i=h;
        return c[b>>2]|0
    }
    function ji(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0;
        h=i;
        i=i+16|0;
        j=h+4|0;
        k=h;
        a=a+8|0;
        a=Qb[c[(c[a>>2]|0)+4>>2]&63](a)|0;
        c[k>>2]=c[e>>2];
        c[j>>2]=c[k>>2];
        d=(qm(d,j,a,a+288|0,g,f,0)|0)-a|0;
        if((d|0)<288)c[b>>2]=((d|0)/12|0|0)%12|0;
        i=h;
        return
    }
    function ki(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+16|0;
        j=h+8|0;
        m=h;
        k=h+4|0;
        l=ff(e)|0;
        c[m>>2]=l;
        e=tk(m,6576)|0;
        Sn(l)|0;
        c[k>>2]=c[d>>2];
        c[j>>2]=c[k>>2];
        li(a,g+20|0,b,j,f,e);
        i=h;
        return c[b>>2]|0
    }
    function li(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,4)|0;
        if(!(c[f>>2]&4))
        {
            if((a|0)<69)a=a+2e3|0;
            else a=(a+-69|0)>>>0<31?a+1900|0:a;
            c[b>>2]=a+-1900
        }
        i=h;
        return
    }
    function mi(b,d,e,f,g,h,j,k)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0;
        S=i;
        i=i+144|0;
        l=S+132|0;
        k=S+128|0;
        L=S+124|0;
        w=S+120|0;
        H=S+116|0;
        M=S+112|0;
        N=S+108|0;
        O=S+104|0;
        P=S+100|0;
        Q=S+96|0;
        R=S+92|0;
        m=S+88|0;
        n=S+84|0;
        o=S+80|0;
        p=S+76|0;
        q=S+72|0;
        r=S+68|0;
        s=S+64|0;
        t=S+24|0;
        u=S;
        v=S+4|0;
        x=S+8|0;
        y=S+12|0;
        z=S+16|0;
        A=S+20|0;
        B=S+28|0;
        C=S+32|0;
        D=S+36|0;
        E=S+40|0;
        F=S+44|0;
        G=S+48|0;
        I=S+52|0;
        J=S+56|0;
        K=S+60|0;
        c[g>>2]=0;
        U=ff(f)|0;
        c[L>>2]=U;
        L=tk(L,6576)|0;
        Sn(U)|0;
        do switch(j<<24>>24|0)
            {
            case 99:
                {
                    U=b+8|0;
                    U=Qb[c[(c[U>>2]|0)+12>>2]&63](U)|0;
                    c[M>>2]=c[d>>2];
                    c[N>>2]=c[e>>2];
                    e=a[U>>0]|0;
                    Q=(e&1)==0;
                    T=U+4|0;
                    U=Q?T:c[U+8>>2]|0;
                    T=U+((Q?(e&255)>>>1:c[T>>2]|0)<<2)|0;
                    c[k>>2]=c[M>>2];
                    c[l>>2]=c[N>>2];
                    c[d>>2]=ai(b,k,l,f,g,h,U,T)|0;
                    T=26;
                    break
                }
            case 101:case 100:
                {
                    c[O>>2]=c[e>>2];
                    c[l>>2]=c[O>>2];
                    ni(b,h+12|0,d,l,g,L);
                    T=26;
                    break
                }
            case 109:
                {
                    c[q>>2]=c[e>>2];
                    c[l>>2]=c[q>>2];
                    ri(b,h+16|0,d,l,g,L);
                    T=26;
                    break
                }
            case 114:
                {
                    c[u>>2]=c[d>>2];
                    c[v>>2]=c[e>>2];
                    c[k>>2]=c[u>>2];
                    c[l>>2]=c[v>>2];
                    c[d>>2]=ai(b,k,l,f,g,h,9776,9820)|0;
                    T=26;
                    break
                }
            case 83:
                {
                    c[z>>2]=c[e>>2];
                    c[l>>2]=c[z>>2];
                    vi(b,h,d,l,g,L);
                    T=26;
                    break
                }
            case 116:case 110:
                {
                    c[s>>2]=c[e>>2];
                    c[l>>2]=c[s>>2];
                    ti(b,d,l,g,L);
                    T=26;
                    break
                }
            case 84:
                {
                    c[A>>2]=c[d>>2];
                    c[B>>2]=c[e>>2];
                    c[k>>2]=c[A>>2];
                    c[l>>2]=c[B>>2];
                    c[d>>2]=ai(b,k,l,f,g,h,9848,9880)|0;
                    T=26;
                    break
                }
            case 119:
                {
                    c[C>>2]=c[e>>2];
                    c[l>>2]=c[C>>2];
                    wi(b,h+24|0,d,l,g,L);
                    T=26;
                    break
                }
            case 120:
                {
                    U=c[(c[b>>2]|0)+20>>2]|0;
                    c[D>>2]=c[d>>2];
                    c[E>>2]=c[e>>2];
                    c[k>>2]=c[D>>2];
                    c[l>>2]=c[E>>2];
                    k=Ob[U&63](b,k,l,f,g,h)|0;
                    break
                }
            case 88:
                {
                    U=b+8|0;
                    U=Qb[c[(c[U>>2]|0)+24>>2]&63](U)|0;
                    c[F>>2]=c[d>>2];
                    c[G>>2]=c[e>>2];
                    e=a[U>>0]|0;
                    Q=(e&1)==0;
                    T=U+4|0;
                    U=Q?T:c[U+8>>2]|0;
                    T=U+((Q?(e&255)>>>1:c[T>>2]|0)<<2)|0;
                    c[k>>2]=c[F>>2];
                    c[l>>2]=c[G>>2];
                    c[d>>2]=ai(b,k,l,f,g,h,U,T)|0;
                    T=26;
                    break
                }
            case 121:
                {
                    c[I>>2]=c[e>>2];
                    c[l>>2]=c[I>>2];
                    li(b,h+20|0,d,l,g,L);
                    T=26;
                    break
                }
            case 68:
                {
                    c[P>>2]=c[d>>2];
                    c[Q>>2]=c[e>>2];
                    c[k>>2]=c[P>>2];
                    c[l>>2]=c[Q>>2];
                    c[d>>2]=ai(b,k,l,f,g,h,9712,9744)|0;
                    T=26;
                    break
                }
            case 89:
                {
                    c[J>>2]=c[e>>2];
                    c[l>>2]=c[J>>2];
                    xi(b,h+20|0,d,l,g,L);
                    T=26;
                    break
                }
            case 37:
                {
                    c[K>>2]=c[e>>2];
                    c[l>>2]=c[K>>2];
                    yi(b,d,l,g,L);
                    T=26;
                    break
                }
            case 104:case 66:case 98:
                {
                    c[H>>2]=c[e>>2];
                    c[l>>2]=c[H>>2];
                    ji(b,h+16|0,d,l,g,L);
                    T=26;
                    break
                }
            case 73:
                {
                    c[o>>2]=c[e>>2];
                    c[l>>2]=c[o>>2];
                    pi(b,h+8|0,d,l,g,L);
                    T=26;
                    break
                }
            case 70:
                {
                    c[R>>2]=c[d>>2];
                    c[m>>2]=c[e>>2];
                    c[k>>2]=c[R>>2];
                    c[l>>2]=c[m>>2];
                    c[d>>2]=ai(b,k,l,f,g,h,9744,9776)|0;
                    T=26;
                    break
                }
            case 72:
                {
                    c[n>>2]=c[e>>2];
                    c[l>>2]=c[n>>2];
                    oi(b,h+8|0,d,l,g,L);
                    T=26;
                    break
                }
            case 106:
                {
                    c[p>>2]=c[e>>2];
                    c[l>>2]=c[p>>2];
                    qi(b,h+28|0,d,l,g,L);
                    T=26;
                    break
                }
            case 77:
                {
                    c[r>>2]=c[e>>2];
                    c[l>>2]=c[r>>2];
                    si(b,h+4|0,d,l,g,L);
                    T=26;
                    break
                }
            case 112:
                {
                    c[t>>2]=c[e>>2];
                    c[l>>2]=c[t>>2];
                    ui(b,h+8|0,d,l,g,L);
                    T=26;
                    break
                }
            case 82:
                {
                    c[x>>2]=c[d>>2];
                    c[y>>2]=c[e>>2];
                    c[k>>2]=c[x>>2];
                    c[l>>2]=c[y>>2];
                    c[d>>2]=ai(b,k,l,f,g,h,9824,9844)|0;
                    T=26;
                    break
                }
            case 65:case 97:
                {
                    c[w>>2]=c[e>>2];
                    c[l>>2]=c[w>>2];
                    hi(b,h+24|0,d,l,g,L);
                    T=26;
                    break
                }
            default:
                {
                    c[g>>2]=c[g>>2]|4;
                    T=26
                }
        }
        while(0);
        if((T|0)==26)k=c[d>>2]|0;
        i=S;
        return k|0
    }
    function ni(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a+-1|0)>>>0<31&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function oi(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a|0)<24&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function pi(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a+-1|0)>>>0<12&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function qi(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,3)|0;
        d=c[f>>2]|0;
        if((a|0)<366&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function ri(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a|0)<13&(d&4|0)==0)c[b>>2]=a+-1;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function si(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a|0)<60&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function ti(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,i=0;
        a:while(1)
            {
            a=c[b>>2]|0;
            do if(a)
            {
                g=c[a+12>>2]|0;
                if((g|0)==(c[a+16>>2]|0))a=Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0;
                else a=c[g>>2]|0;
                if((a|0)==-1)
                {
                    c[b>>2]=0;
                    h=1;
                    break
                }
                else
                {
                    h=(c[b>>2]|0)==0;
                    break
                }
            }
            else h=1;
            while(0);
            g=c[d>>2]|0;
            do if(g)
            {
                a=c[g+12>>2]|0;
                if((a|0)==(c[g+16>>2]|0))a=Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0;
                else a=c[a>>2]|0;
                if((a|0)!=-1)if(h)
                {
                    h=g;
                    break
                }
                else
                {
                    h=g;
                    break a
                }
                else
                {
                    c[d>>2]=0;
                    i=15;
                    break
                }
            }
            else i=15;
            while(0);
            if((i|0)==15)
            {
                i=0;
                if(h)
                {
                    h=0;
                    break
                }
                else h=0
            }
            a=c[b>>2]|0;
            g=c[a+12>>2]|0;
            if((g|0)==(c[a+16>>2]|0))a=Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0;
            else a=c[g>>2]|0;
            if(!(Jb[c[(c[f>>2]|0)+12>>2]&31](f,8192,a)|0))break;
            a=c[b>>2]|0;
            g=a+12|0;
            h=c[g>>2]|0;
            if((h|0)==(c[a+16>>2]|0))
            {
                Qb[c[(c[a>>2]|0)+40>>2]&63](a)|0;
                continue
            }
            else
            {
                c[g>>2]=h+4;
                continue
            }
        }
        a=c[b>>2]|0;
        do if(a)
        {
            g=c[a+12>>2]|0;
            if((g|0)==(c[a+16>>2]|0))a=Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0;
            else a=c[g>>2]|0;
            if((a|0)==-1)
            {
                c[b>>2]=0;
                g=1;
                break
            }
            else
            {
                g=(c[b>>2]|0)==0;
                break
            }
        }
        else g=1;
        while(0);
        do if(h)
        {
            a=c[h+12>>2]|0;
            if((a|0)==(c[h+16>>2]|0))a=Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0;
            else a=c[a>>2]|0;
            if((a|0)!=-1)if(g)break;
            else
            {
                i=39;
                break
            }
            else
            {
                c[d>>2]=0;
                i=37;
                break
            }
        }
        else i=37;
        while(0);
        if((i|0)==37?g:0)i=39;
        if((i|0)==39)c[e>>2]=c[e>>2]|2;
        return
    }
    function ui(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0;
        n=i;
        i=i+16|0;
        k=n+4|0;
        l=n;
        m=b+8|0;
        m=Qb[c[(c[m>>2]|0)+8>>2]&63](m)|0;
        b=a[m>>0]|0;
        if(!(b&1))j=(b&255)>>>1;
        else j=c[m+4>>2]|0;
        b=a[m+12>>0]|0;
        if(!(b&1))b=(b&255)>>>1;
        else b=c[m+16>>2]|0;
        do if((j|0)!=(0-b|0))
        {
            c[l>>2]=c[f>>2];
            c[k>>2]=c[l>>2];
            b=qm(e,k,m,m+24|0,h,g,0)|0;
            j=c[d>>2]|0;
            if((b|0)==(m|0)&(j|0)==12)
            {
                c[d>>2]=0;
                break
            }
            if((j|0)<12&(b-m|0)==12)c[d>>2]=j+12
        }
        else c[g>>2]=c[g>>2]|4;
        while(0);
        i=n;
        return
    }
    function vi(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,2)|0;
        d=c[f>>2]|0;
        if((a|0)<61&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function wi(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,1)|0;
        d=c[f>>2]|0;
        if((a|0)<7&(d&4|0)==0)c[b>>2]=a;
        else c[f>>2]=d|4;
        i=h;
        return
    }
    function xi(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        h=i;
        i=i+16|0;
        a=h+4|0;
        j=h;
        c[j>>2]=c[e>>2];
        c[a>>2]=c[j>>2];
        a=Em(d,a,f,g,4)|0;
        if(!(c[f>>2]&4))c[b>>2]=a+-1900;
        i=h;
        return
    }
    function yi(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,i=0,j=0;
        a=c[b>>2]|0;
        do if(a)
        {
            g=c[a+12>>2]|0;
            if((g|0)==(c[a+16>>2]|0))a=Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0;
            else a=c[g>>2]|0;
            if((a|0)==-1)
            {
                c[b>>2]=0;
                h=1;
                break
            }
            else
            {
                h=(c[b>>2]|0)==0;
                break
            }
        }
        else h=1;
        while(0);
        g=c[d>>2]|0;
        do if(g)
        {
            a=c[g+12>>2]|0;
            if((a|0)==(c[g+16>>2]|0))a=Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0;
            else a=c[a>>2]|0;
            if((a|0)!=-1)if(h)
            {
                i=g;
                j=17;
                break
            }
            else
            {
                j=16;
                break
            }
            else
            {
                c[d>>2]=0;
                j=14;
                break
            }
        }
        else j=14;
        while(0);
        if((j|0)==14)if(h)j=16;
        else
        {
            i=0;
            j=17
        }
        a:do if((j|0)==16)c[e>>2]=c[e>>2]|6;
        else if((j|0)==17)
        {
            a=c[b>>2]|0;
            g=c[a+12>>2]|0;
            if((g|0)==(c[a+16>>2]|0))a=Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0;
            else a=c[g>>2]|0;
            if((Jb[c[(c[f>>2]|0)+52>>2]&31](f,a,0)|0)<<24>>24!=37)
            {
                c[e>>2]=c[e>>2]|4;
                break
            }
            a=c[b>>2]|0;
            g=a+12|0;
            h=c[g>>2]|0;
            if((h|0)==(c[a+16>>2]|0))
            {
                Qb[c[(c[a>>2]|0)+40>>2]&63](a)|0;
                a=c[b>>2]|0;
                if(!a)g=1;
                else j=25
            }
            else
            {
                c[g>>2]=h+4;
                j=25
            }
            do if((j|0)==25)
            {
                g=c[a+12>>2]|0;
                if((g|0)==(c[a+16>>2]|0))a=Qb[c[(c[a>>2]|0)+36>>2]&63](a)|0;
                else a=c[g>>2]|0;
                if((a|0)==-1)
                {
                    c[b>>2]=0;
                    g=1;
                    break
                }
                else
                {
                    g=(c[b>>2]|0)==0;
                    break
                }
            }
            while(0);
            do if(i)
            {
                a=c[i+12>>2]|0;
                if((a|0)==(c[i+16>>2]|0))a=Qb[c[(c[i>>2]|0)+36>>2]&63](i)|0;
                else a=c[a>>2]|0;
                if((a|0)!=-1)if(g)break a;
                else break;
                else
                {
                    c[d>>2]=0;
                    j=37;
                    break
                }
            }
            else j=37;
            while(0);
            if((j|0)==37?!g:0)break;
            c[e>>2]=c[e>>2]|2
        }
        while(0);
        return
    }
    function zi(a)
    {
        a=a|0;
        Ai(a+8|0);
        return
    }
    function Ai(a)
    {
        a=a|0;
        var b=0;
        b=c[a>>2]|0;
        if((b|0)!=(Jg()|0))bb(c[a>>2]|0);
        return
    }
    function Bi(a)
    {
        a=a|0;
        Ai(a+8|0);
        Lc(a);
        return
    }
    function Ci(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0;
        l=i;
        i=i+112|0;
        k=l+4|0;
        e=l;
        c[e>>2]=k+100;
        Di(b+8|0,k,e,g,h,j);
        g=c[e>>2]|0;
        e=c[d>>2]|0;
        if((k|0)!=(g|0))do
            {
            h=a[k>>0]|0;
            do if(e)
            {
                f=e+24|0;
                j=c[f>>2]|0;
                if((j|0)==(c[e+28>>2]|0))
                {
                    b=(Wb[c[(c[e>>2]|0)+52>>2]&15](e,h&255)|0)==-1;
                    e=b?0:e;
                    break
                }
                else
                {
                    c[f>>2]=j+1;
                    a[j>>0]=h;
                    break
                }
            }
            else e=0;
            while(0);
            k=k+1|0
        }
        while((k|0)!=(g|0));
        i=l;
        return e|0
    }
    function Di(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0;
        m=i;
        i=i+16|0;
        l=m;
        a[l>>0]=37;
        j=l+1|0;
        a[j>>0]=g;
        k=l+2|0;
        a[k>>0]=h;
        a[l+3>>0]=0;
        if(h<<24>>24)
        {
            a[j>>0]=h;
            a[k>>0]=g
        }
        c[e>>2]=d+(Ra(d|0,(c[e>>2]|0)-d|0,l|0,f|0,c[b>>2]|0)|0);
        i=m;
        return
    }
    function Ei(a)
    {
        a=a|0;
        Ai(a+8|0);
        return
    }
    function Fi(a)
    {
        a=a|0;
        Ai(a+8|0);
        Lc(a);
        return
    }
    function Gi(a,b,d,e,f,g,h)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0;
        j=i;
        i=i+416|0;
        e=j+8|0;
        d=j;
        c[d>>2]=e+400;
        Hi(a+8|0,e,d,f,g,h);
        f=c[d>>2]|0;
        d=c[b>>2]|0;
        if((e|0)!=(f|0))
        {
            a=e;
            do
            {
                e=c[a>>2]|0;
                if(!d)d=0;
                else
                {
                    h=d+24|0;
                    g=c[h>>2]|0;
                    if((g|0)==(c[d+28>>2]|0))e=Wb[c[(c[d>>2]|0)+52>>2]&15](d,e)|0;
                    else
                    {
                        c[h>>2]=g+4;
                        c[g>>2]=e
                    }
                    d=(e|0)==-1?0:d
                }
                a=a+4|0
            }
            while((a|0)!=(f|0))
        }
        i=j;
        return d|0
    }
    function Hi(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0,k=0,l=0,m=0;
        h=i;
        i=i+128|0;
        l=h+16|0;
        m=h+8|0;
        j=h;
        k=h+12|0;
        c[m>>2]=l+100;
        Di(a,l,m,e,f,g);
        g=j;
        c[g>>2]=0;
        c[g+4>>2]=0;
        c[k>>2]=l;
        g=(c[d>>2]|0)-b>>2;
        f=Va(c[a>>2]|0)|0;
        g=tc(b,k,g,j)|0;
        if(f)Va(f|0)|0;
        c[d>>2]=b+(g<<2);
        i=h;
        return
    }
    function Ii(a)
    {
        a=a|0;
        return
    }
    function Ji(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Ki(a)
    {
        a=a|0;
        return 127
    }
    function Li(a)
    {
        a=a|0;
        return 127
    }
    function Mi(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function Ni(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function Oi(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function Pi(a,b)
    {
        a=a|0;
        b=b|0;
        Je(a,1,45);
        return
    }
    function Qi(a)
    {
        a=a|0;
        return 0
    }
    function Ri(b,c)
    {
        b=b|0;
        c=c|0;
        a[b>>0]=2;
        a[b+1>>0]=3;
        a[b+2>>0]=0;
        a[b+3>>0]=4;
        return
    }
    function Si(b,c)
    {
        b=b|0;
        c=c|0;
        a[b>>0]=2;
        a[b+1>>0]=3;
        a[b+2>>0]=0;
        a[b+3>>0]=4;
        return
    }
    function Ti(a)
    {
        a=a|0;
        return
    }
    function Ui(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Vi(a)
    {
        a=a|0;
        return 127
    }
    function Wi(a)
    {
        a=a|0;
        return 127
    }
    function Xi(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function Yi(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function Zi(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function _i(a,b)
    {
        a=a|0;
        b=b|0;
        Je(a,1,45);
        return
    }
    function $i(a)
    {
        a=a|0;
        return 0
    }
    function aj(b,c)
    {
        b=b|0;
        c=c|0;
        a[b>>0]=2;
        a[b+1>>0]=3;
        a[b+2>>0]=0;
        a[b+3>>0]=4;
        return
    }
    function bj(b,c)
    {
        b=b|0;
        c=c|0;
        a[b>>0]=2;
        a[b+1>>0]=3;
        a[b+2>>0]=0;
        a[b+3>>0]=4;
        return
    }
    function cj(a)
    {
        a=a|0;
        return
    }
    function dj(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function ej(a)
    {
        a=a|0;
        return 2147483647
    }
    function fj(a)
    {
        a=a|0;
        return 2147483647
    }
    function gj(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function hj(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function ij(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function jj(a,b)
    {
        a=a|0;
        b=b|0;
        Ue(a,1,45);
        return
    }
    function kj(a)
    {
        a=a|0;
        return 0
    }
    function lj(b,c)
    {
        b=b|0;
        c=c|0;
        a[b>>0]=2;
        a[b+1>>0]=3;
        a[b+2>>0]=0;
        a[b+3>>0]=4;
        return
    }
    function mj(b,c)
    {
        b=b|0;
        c=c|0;
        a[b>>0]=2;
        a[b+1>>0]=3;
        a[b+2>>0]=0;
        a[b+3>>0]=4;
        return
    }
    function nj(a)
    {
        a=a|0;
        return
    }
    function oj(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function pj(a)
    {
        a=a|0;
        return 2147483647
    }
    function qj(a)
    {
        a=a|0;
        return 2147483647
    }
    function rj(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function sj(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function tj(a,b)
    {
        a=a|0;
        b=b|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function uj(a,b)
    {
        a=a|0;
        b=b|0;
        Ue(a,1,45);
        return
    }
    function vj(a)
    {
        a=a|0;
        return 0
    }
    function wj(b,c)
    {
        b=b|0;
        c=c|0;
        a[b>>0]=2;
        a[b+1>>0]=3;
        a[b+2>>0]=0;
        a[b+3>>0]=4;
        return
    }
    function xj(b,c)
    {
        b=b|0;
        c=c|0;
        a[b>>0]=2;
        a[b+1>>0]=3;
        a[b+2>>0]=0;
        a[b+3>>0]=4;
        return
    }
    function yj(a)
    {
        a=a|0;
        return
    }
    function zj(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Aj(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0;
        F=i;
        i=i+240|0;
        y=F+16|0;
        z=F;
        v=F+128|0;
        E=F+120|0;
        x=F+12|0;
        B=F+8|0;
        k=F+238|0;
        t=F+4|0;
        w=F+228|0;
        c[E>>2]=v;
        D=E+4|0;
        c[D>>2]=96;
        c[B>>2]=ff(g)|0;
        b=tk(B,6584)|0;
        a[k>>0]=0;
        c[t>>2]=c[e>>2];
        u=c[g+4>>2]|0;
        c[y>>2]=c[t>>2];
        if(Cj(d,y,f,B,u,h,k,b,E,x,v+100|0)|0)
        {
            Ub[c[(c[b>>2]|0)+32>>2]&7](b,9880,9890,w)|0;
            f=c[x>>2]|0;
            t=c[E>>2]|0;
            b=f-t|0;
            if((b|0)>98)
            {
                b=$d(b+2|0)|0;
                if(!b)sd();
                else
                {
                    A=b;
                    l=b
                }
            }
            else
            {
                A=0;
                l=y
            }
            if(!(a[k>>0]|0))b=l;
            else
            {
                a[l>>0]=45;
                b=l+1|0
            }
            u=w+10|0;
            v=w;
            if(t>>>0<f>>>0)
            {
                g=w+1|0;
                l=g+1|0;
                m=l+1|0;
                n=m+1|0;
                o=n+1|0;
                p=o+1|0;
                q=p+1|0;
                r=q+1|0;
                s=r+1|0;
                f=t;
                do
                {
                    k=a[f>>0]|0;
                    if((a[w>>0]|0)!=k<<24>>24)if((a[g>>0]|0)!=k<<24>>24)if((a[l>>0]|0)!=k<<24>>24)if((a[m>>0]|0)!=k<<24>>24)if((a[n>>0]|0)!=k<<24>>24)if((a[o>>0]|0)!=k<<24>>24)if((a[p>>0]|0)!=k<<24>>24)if((a[q>>0]|0)!=k<<24>>24)if((a[r>>0]|0)==k<<24>>24)k=r;
                    else k=(a[s>>0]|0)==k<<24>>24?s:u;
                    else k=q;
                    else k=p;
                    else k=o;
                    else k=n;
                    else k=m;
                    else k=l;
                    else k=g;
                    else k=w;
                    a[b>>0]=a[9880+(k-v)>>0]|0;
                    f=f+1|0;
                    b=b+1|0
                }
                while(f>>>0<(c[x>>2]|0)>>>0)
            }
            a[b>>0]=0;
            c[z>>2]=j;
            yc(y,9896,z)|0;
            if(A)ae(A)
        }
        b=c[d>>2]|0;
        do if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0))if((Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1)
            {
                c[d>>2]=0;
                b=0;
                break
            }
            else
            {
                b=c[d>>2]|0;
                break
            }
        }
        else b=0;
        while(0);
        b=(b|0)==0;
        f=c[e>>2]|0;
        do if(f)
        {
            if((c[f+12>>2]|0)==(c[f+16>>2]|0)?(Qb[c[(c[f>>2]|0)+36>>2]&63](f)|0)==-1:0)
            {
                c[e>>2]=0;
                C=25;
                break
            }
            if(!b)C=26
        }
        else C=25;
        while(0);
        if((C|0)==25?b:0)C=26;
        if((C|0)==26)c[h>>2]=c[h>>2]|2;
        f=c[d>>2]|0;
        Sn(c[B>>2]|0)|0;
        b=c[E>>2]|0;
        c[E>>2]=0;
        if(b)Mb[c[D>>2]&127](b);
        i=F;
        return f|0
    }
    function Bj(a)
    {
        a=a|0;
        return
    }
    function Cj(e,f,g,h,j,k,l,m,n,o,p)
    {
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        n=n|0;
        o=o|0;
        p=p|0;
        var q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ba=0,ca=0;
        ca=i;
        i=i+496|0;
        O=ca+68|0;
        t=ca+88|0;
        ba=ca+72|0;
        S=ca+84|0;
        R=ca+80|0;
        T=ca+488|0;
        P=ca+493|0;
        U=ca+492|0;
        Y=ca+52|0;
        aa=ca+40|0;
        _=ca+28|0;
        Z=ca+16|0;
        $=ca+4|0;
        Q=ca;
        W=ca+64|0;
        c[O>>2]=p;
        c[ba>>2]=t;
        X=ba+4|0;
        c[X>>2]=96;
        c[S>>2]=t;
        c[R>>2]=t+400;
        c[Y>>2]=0;
        c[Y+4>>2]=0;
        c[Y+8>>2]=0;
        c[aa>>2]=0;
        c[aa+4>>2]=0;
        c[aa+8>>2]=0;
        c[_>>2]=0;
        c[_+4>>2]=0;
        c[_+8>>2]=0;
        c[Z>>2]=0;
        c[Z+4>>2]=0;
        c[Z+8>>2]=0;
        c[$>>2]=0;
        c[$+4>>2]=0;
        c[$+8>>2]=0;
        Fj(g,h,T,P,U,Y,aa,_,Z,Q);
        c[o>>2]=c[n>>2];
        H=m+8|0;
        I=_+4|0;
        J=Z+4|0;
        K=Z+8|0;
        L=Z+1|0;
        M=_+8|0;
        N=_+1|0;
        x=(j&512|0)!=0;
        y=aa+8|0;
        z=aa+1|0;
        A=aa+4|0;
        B=$+4|0;
        C=$+8|0;
        D=$+1|0;
        E=T+3|0;
        F=Y+4|0;
        G=0;
        j=0;
        a:while(1)
            {
            p=c[e>>2]|0;
            do if(p)
            {
                if((c[p+12>>2]|0)==(c[p+16>>2]|0))if((Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0)==-1)
                {
                    c[e>>2]=0;
                    p=0;
                    break
                }
                else
                {
                    p=c[e>>2]|0;
                    break
                }
            }
            else p=0;
            while(0);
            p=(p|0)==0;
            m=c[f>>2]|0;
            do if(m)
            {
                if((c[m+12>>2]|0)!=(c[m+16>>2]|0))if(p)break;
                else
                {
                    V=202;
                    break a
                }
                if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)!=-1)if(p)break;
                else
                {
                    V=202;
                    break a
                }
                else
                {
                    c[f>>2]=0;
                    V=12;
                    break
                }
            }
            else V=12;
            while(0);
            if((V|0)==12)
            {
                V=0;
                if(p)
                {
                    V=202;
                    break
                }
                else m=0
            }
            b:do switch(a[T+G>>0]|0)
                {
                case 2:
                    {
                        if(!(G>>>0<2|(j|0)!=0)?!(x|(G|0)==2&(a[E>>0]|0)!=0):0)
                        {
                            j=0;
                            break b
                        }
                        v=a[aa>>0]|0;
                        p=(v&1)==0;
                        w=c[y>>2]|0;
                        h=p?z:w;
                        u=h;
                        c:do if((G|0)!=0?(d[T+(G+-1)>>0]|0)<2:0)
                        {
                            s=p?(v&255)>>>1:c[A>>2]|0;
                            r=h+s|0;
                            q=c[H>>2]|0;
                            d:do if(!s)g=u;
                            else
                            {
                                s=h;
                                g=u;
                                do
                                {
                                    p=a[s>>0]|0;
                                    if(p<<24>>24<=-1)break d;
                                    if(!(b[q+(p<<24>>24<<1)>>1]&8192))break d;
                                    s=s+1|0;
                                    g=s
                                }
                                while((s|0)!=(r|0))
                            }
                            while(0);
                            r=g-u|0;
                            q=a[$>>0]|0;
                            p=(q&1)==0;
                            q=p?(q&255)>>>1:c[B>>2]|0;
                            if(q>>>0>=r>>>0)
                            {
                                p=p?D:c[C>>2]|0;
                                s=p+q|0;
                                if((g|0)!=(u|0))
                                {
                                    p=p+(q-r)|0;
                                    while(1)
                                    {
                                        if((a[p>>0]|0)!=(a[h>>0]|0))
                                        {
                                            g=u;
                                            break c
                                        }
                                        p=p+1|0;
                                        if((p|0)==(s|0))break;
                                        else h=h+1|0
                                    }
                                }
                            }
                            else g=u
                        }
                        else g=u;
                        while(0);
                        p=(v&1)==0;
                        p=(p?z:w)+(p?(v&255)>>>1:c[A>>2]|0)|0;
                        e:do if((g|0)!=(p|0))
                        {
                            p=m;
                            h=m;
                            while(1)
                            {
                                m=c[e>>2]|0;
                                do if(m)
                                {
                                    if((c[m+12>>2]|0)==(c[m+16>>2]|0))if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1)
                                    {
                                        c[e>>2]=0;
                                        m=0;
                                        break
                                    }
                                    else
                                    {
                                        m=c[e>>2]|0;
                                        break
                                    }
                                }
                                else m=0;
                                while(0);
                                m=(m|0)==0;
                                do if(h)
                                {
                                    if((c[h+12>>2]|0)!=(c[h+16>>2]|0))if(m)
                                    {
                                        q=p;
                                        r=h;
                                        break
                                    }
                                    else
                                    {
                                        p=g;
                                        break e
                                    }
                                    if((Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0)!=-1)if(m^(p|0)==0)
                                    {
                                        q=p;
                                        r=p;
                                        break
                                    }
                                    else
                                    {
                                        p=g;
                                        break e
                                    }
                                    else
                                    {
                                        c[f>>2]=0;
                                        p=0;
                                        V=107;
                                        break
                                    }
                                }
                                else V=107;
                                while(0);
                                if((V|0)==107)
                                {
                                    V=0;
                                    if(m)
                                    {
                                        p=g;
                                        break e
                                    }
                                    else
                                    {
                                        q=p;
                                        r=0
                                    }
                                }
                                p=c[e>>2]|0;
                                m=c[p+12>>2]|0;
                                if((m|0)==(c[p+16>>2]|0))p=Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0;
                                else p=d[m>>0]|0;
                                if((p&255)<<24>>24!=(a[g>>0]|0))
                                {
                                    p=g;
                                    break e
                                }
                                p=c[e>>2]|0;
                                m=p+12|0;
                                h=c[m>>2]|0;
                                if((h|0)==(c[p+16>>2]|0))Qb[c[(c[p>>2]|0)+40>>2]&63](p)|0;
                                else c[m>>2]=h+1;
                                g=g+1|0;
                                p=a[aa>>0]|0;
                                w=(p&1)==0;
                                p=(w?z:c[y>>2]|0)+(w?(p&255)>>>1:c[A>>2]|0)|0;
                                if((g|0)==(p|0))break;
                                else
                                {
                                    p=q;
                                    h=r
                                }
                            }
                        }
                        while(0);
                        if(x?(w=a[aa>>0]|0,v=(w&1)==0,(p|0)!=((v?z:c[y>>2]|0)+(v?(w&255)>>>1:c[A>>2]|0)|0)):0)
                        {
                            V=119;
                            break a
                        }
                        break
                    }
                case 3:
                    {
                        h=a[_>>0]|0;
                        p=(h&1)==0?(h&255)>>>1:c[I>>2]|0;
                        g=a[Z>>0]|0;
                        g=(g&1)==0?(g&255)>>>1:c[J>>2]|0;
                        if((p|0)!=(0-g|0))
                        {
                            r=(p|0)==0;
                            q=c[e>>2]|0;
                            s=c[q+12>>2]|0;
                            p=c[q+16>>2]|0;
                            m=(s|0)==(p|0);
                            if(r|(g|0)==0)
                            {
                                if(m)p=Qb[c[(c[q>>2]|0)+36>>2]&63](q)|0;
                                else p=d[s>>0]|0;
                                p=p&255;
                                if(r)
                                {
                                    if(p<<24>>24!=(a[((a[Z>>0]&1)==0?L:c[K>>2]|0)>>0]|0))break b;
                                    p=c[e>>2]|0;
                                    m=p+12|0;
                                    g=c[m>>2]|0;
                                    if((g|0)==(c[p+16>>2]|0))Qb[c[(c[p>>2]|0)+40>>2]&63](p)|0;
                                    else c[m>>2]=g+1;
                                    a[l>>0]=1;
                                    w=a[Z>>0]|0;
                                    j=((w&1)==0?(w&255)>>>1:c[J>>2]|0)>>>0>1?Z:j;
                                    break b
                                }
                                if(p<<24>>24!=(a[((a[_>>0]&1)==0?N:c[M>>2]|0)>>0]|0))
                                {
                                    a[l>>0]=1;
                                    break b
                                }
                                p=c[e>>2]|0;
                                m=p+12|0;
                                g=c[m>>2]|0;
                                if((g|0)==(c[p+16>>2]|0))Qb[c[(c[p>>2]|0)+40>>2]&63](p)|0;
                                else c[m>>2]=g+1;
                                w=a[_>>0]|0;
                                j=((w&1)==0?(w&255)>>>1:c[I>>2]|0)>>>0>1?_:j;
                                break b
                            }
                            if(m)
                            {
                                r=Qb[c[(c[q>>2]|0)+36>>2]&63](q)|0;
                                p=c[e>>2]|0;
                                h=a[_>>0]|0;
                                q=p;
                                g=c[p+12>>2]|0;
                                p=c[p+16>>2]|0
                            }
                            else
                            {
                                r=d[s>>0]|0;
                                g=s
                            }
                            m=q+12|0;
                            p=(g|0)==(p|0);
                            if((r&255)<<24>>24==(a[((h&1)==0?N:c[M>>2]|0)>>0]|0))
                            {
                                if(p)Qb[c[(c[q>>2]|0)+40>>2]&63](q)|0;
                                else c[m>>2]=g+1;
                                w=a[_>>0]|0;
                                j=((w&1)==0?(w&255)>>>1:c[I>>2]|0)>>>0>1?_:j;
                                break b
                            }
                            if(p)p=Qb[c[(c[q>>2]|0)+36>>2]&63](q)|0;
                            else p=d[g>>0]|0;
                            if((p&255)<<24>>24!=(a[((a[Z>>0]&1)==0?L:c[K>>2]|0)>>0]|0))
                            {
                                V=82;
                                break a
                            }
                            p=c[e>>2]|0;
                            m=p+12|0;
                            g=c[m>>2]|0;
                            if((g|0)==(c[p+16>>2]|0))Qb[c[(c[p>>2]|0)+40>>2]&63](p)|0;
                            else c[m>>2]=g+1;
                            a[l>>0]=1;
                            w=a[Z>>0]|0;
                            j=((w&1)==0?(w&255)>>>1:c[J>>2]|0)>>>0>1?Z:j
                        }
                        break
                    }
                case 0:
                    {
                        if((G|0)!=3)
                        {
                            p=m;
                            g=m;
                            V=28
                        }
                        break
                    }
                case 4:
                    {
                        s=a[U>>0]|0;
                        r=m;
                        h=m;
                        p=0;
                        f:while(1)
                            {
                            m=c[e>>2]|0;
                            do if(m)
                            {
                                if((c[m+12>>2]|0)==(c[m+16>>2]|0))if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1)
                                {
                                    c[e>>2]=0;
                                    m=0;
                                    break
                                }
                                else
                                {
                                    m=c[e>>2]|0;
                                    break
                                }
                            }
                            else m=0;
                            while(0);
                            g=(m|0)==0;
                            do if(h)
                            {
                                if((c[h+12>>2]|0)!=(c[h+16>>2]|0))if(g)
                                {
                                    m=r;
                                    q=h;
                                    break
                                }
                                else
                                {
                                    m=r;
                                    break f
                                }
                                if((Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0)!=-1)if(g^(r|0)==0)
                                {
                                    m=r;
                                    q=r;
                                    break
                                }
                                else
                                {
                                    m=r;
                                    break f
                                }
                                else
                                {
                                    c[f>>2]=0;
                                    m=0;
                                    V=130;
                                    break
                                }
                            }
                            else
                            {
                                m=r;
                                V=130
                            }
                            while(0);
                            if((V|0)==130)
                            {
                                V=0;
                                if(g)break;
                                else q=0
                            }
                            g=c[e>>2]|0;
                            h=c[g+12>>2]|0;
                            if((h|0)==(c[g+16>>2]|0))g=Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0;
                            else g=d[h>>0]|0;
                            h=g&255;
                            if(h<<24>>24>-1?(b[(c[H>>2]|0)+(g<<24>>24<<1)>>1]&2048)!=0:0)
                            {
                                g=c[o>>2]|0;
                                if((g|0)==(c[O>>2]|0))
                                {
                                    Fm(n,o,O);
                                    g=c[o>>2]|0
                                }
                                c[o>>2]=g+1;
                                a[g>>0]=h;
                                p=p+1|0
                            }
                            else
                            {
                                w=a[Y>>0]|0;
                                if(!(h<<24>>24==s<<24>>24&((p|0)!=0?(((w&1)==0?(w&255)>>>1:c[F>>2]|0)|0)!=0:0)))break;
                                if((t|0)==(c[R>>2]|0))
                                {
                                    Gm(ba,S,R);
                                    t=c[S>>2]|0
                                }
                                w=t+4|0;
                                c[S>>2]=w;
                                c[t>>2]=p;
                                t=w;
                                p=0
                            }
                            g=c[e>>2]|0;
                            h=g+12|0;
                            r=c[h>>2]|0;
                            if((r|0)==(c[g+16>>2]|0))
                            {
                                Qb[c[(c[g>>2]|0)+40>>2]&63](g)|0;
                                r=m;
                                h=q;
                                continue
                            }
                            else
                            {
                                c[h>>2]=r+1;
                                r=m;
                                h=q;
                                continue
                            }
                        }
                        if((p|0)!=0?(c[ba>>2]|0)!=(t|0):0)
                        {
                            if((t|0)==(c[R>>2]|0))
                            {
                                Gm(ba,S,R);
                                t=c[S>>2]|0
                            }
                            w=t+4|0;
                            c[S>>2]=w;
                            c[t>>2]=p;
                            t=w
                        }
                        q=c[Q>>2]|0;
                        if((q|0)>0)
                        {
                            p=c[e>>2]|0;
                            do if(p)
                            {
                                if((c[p+12>>2]|0)==(c[p+16>>2]|0))if((Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0)==-1)
                                {
                                    c[e>>2]=0;
                                    p=0;
                                    break
                                }
                                else
                                {
                                    p=c[e>>2]|0;
                                    break
                                }
                            }
                            else p=0;
                            while(0);
                            p=(p|0)==0;
                            do if(m)
                            {
                                if((c[m+12>>2]|0)==(c[m+16>>2]|0)?(Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1:0)
                                {
                                    c[f>>2]=0;
                                    V=162;
                                    break
                                }
                                if(p)h=m;
                                else
                                {
                                    V=167;
                                    break a
                                }
                            }
                            else V=162;
                            while(0);
                            if((V|0)==162)
                            {
                                V=0;
                                if(p)
                                {
                                    V=167;
                                    break a
                                }
                                else h=0
                            }
                            p=c[e>>2]|0;
                            m=c[p+12>>2]|0;
                            if((m|0)==(c[p+16>>2]|0))p=Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0;
                            else p=d[m>>0]|0;
                            if((p&255)<<24>>24!=(a[P>>0]|0))
                            {
                                V=167;
                                break a
                            }
                            p=c[e>>2]|0;
                            m=p+12|0;
                            g=c[m>>2]|0;
                            if((g|0)==(c[p+16>>2]|0))Qb[c[(c[p>>2]|0)+40>>2]&63](p)|0;
                            else c[m>>2]=g+1;
                            if((q|0)>0)
                            {
                                r=h;
                                g=h;
                                while(1)
                                {
                                    p=c[e>>2]|0;
                                    do if(p)
                                    {
                                        if((c[p+12>>2]|0)==(c[p+16>>2]|0))if((Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0)==-1)
                                        {
                                            c[e>>2]=0;
                                            p=0;
                                            break
                                        }
                                        else
                                        {
                                            p=c[e>>2]|0;
                                            break
                                        }
                                    }
                                    else p=0;
                                    while(0);
                                    m=(p|0)==0;
                                    do if(g)
                                    {
                                        if((c[g+12>>2]|0)!=(c[g+16>>2]|0))if(m)
                                        {
                                            p=r;
                                            s=g;
                                            break
                                        }
                                        else
                                        {
                                            V=189;
                                            break a
                                        }
                                        if((Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0)!=-1)if(m^(r|0)==0)
                                        {
                                            p=r;
                                            s=r;
                                            break
                                        }
                                        else
                                        {
                                            V=189;
                                            break a
                                        }
                                        else
                                        {
                                            c[f>>2]=0;
                                            p=0;
                                            V=182;
                                            break
                                        }
                                    }
                                    else
                                    {
                                        p=r;
                                        V=182
                                    }
                                    while(0);
                                    if((V|0)==182)
                                    {
                                        V=0;
                                        if(m)
                                        {
                                            V=189;
                                            break a
                                        }
                                        else s=0
                                    }
                                    m=c[e>>2]|0;
                                    g=c[m+12>>2]|0;
                                    if((g|0)==(c[m+16>>2]|0))m=Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0;
                                    else m=d[g>>0]|0;
                                    if((m&255)<<24>>24<=-1)
                                    {
                                        V=189;
                                        break a
                                    }
                                    if(!(b[(c[H>>2]|0)+(m<<24>>24<<1)>>1]&2048))
                                    {
                                        V=189;
                                        break a
                                    }
                                    if((c[o>>2]|0)==(c[O>>2]|0))Fm(n,o,O);
                                    m=c[e>>2]|0;
                                    g=c[m+12>>2]|0;
                                    if((g|0)==(c[m+16>>2]|0))m=Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0;
                                    else m=d[g>>0]|0;
                                    g=c[o>>2]|0;
                                    c[o>>2]=g+1;
                                    a[g>>0]=m;
                                    m=q;
                                    q=q+-1|0;
                                    c[Q>>2]=q;
                                    g=c[e>>2]|0;
                                    h=g+12|0;
                                    r=c[h>>2]|0;
                                    if((r|0)==(c[g+16>>2]|0))Qb[c[(c[g>>2]|0)+40>>2]&63](g)|0;
                                    else c[h>>2]=r+1;
                                    if((m|0)<=1)break;
                                    else
                                    {
                                        r=p;
                                        g=s
                                    }
                                }
                            }
                        }
                        if((c[o>>2]|0)==(c[n>>2]|0))
                        {
                            V=200;
                            break a
                        }
                        break
                    }
                case 1:
                    {
                        if((G|0)!=3)
                        {
                            p=c[e>>2]|0;
                            g=c[p+12>>2]|0;
                            if((g|0)==(c[p+16>>2]|0))p=Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0;
                            else p=d[g>>0]|0;
                            if((p&255)<<24>>24<=-1)
                            {
                                V=26;
                                break a
                            }
                            if(!(b[(c[H>>2]|0)+(p<<24>>24<<1)>>1]&8192))
                            {
                                V=26;
                                break a
                            }
                            p=c[e>>2]|0;
                            g=p+12|0;
                            h=c[g>>2]|0;
                            if((h|0)==(c[p+16>>2]|0))p=Qb[c[(c[p>>2]|0)+40>>2]&63](p)|0;
                            else
                            {
                                c[g>>2]=h+1;
                                p=d[h>>0]|0
                            }
                            Qe($,p&255);
                            p=m;
                            g=m;
                            V=28
                        }
                        break
                    }
                default:
                    {
                    }
            }
            while(0);
            g:do if((V|0)==28)while(1)
                {
                V=0;
                m=c[e>>2]|0;
                do if(m)
                {
                    if((c[m+12>>2]|0)==(c[m+16>>2]|0))if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1)
                    {
                        c[e>>2]=0;
                        m=0;
                        break
                    }
                    else
                    {
                        m=c[e>>2]|0;
                        break
                    }
                }
                else m=0;
                while(0);
                m=(m|0)==0;
                do if(g)
                {
                    if((c[g+12>>2]|0)!=(c[g+16>>2]|0))if(m)
                    {
                        r=p;
                        h=g;
                        break
                    }
                    else break g;
                    if((Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0)!=-1)if(m^(p|0)==0)
                    {
                        r=p;
                        h=p;
                        break
                    }
                    else break g;
                    else
                    {
                        c[f>>2]=0;
                        p=0;
                        V=38;
                        break
                    }
                }
                else V=38;
                while(0);
                if((V|0)==38)
                {
                    V=0;
                    if(m)break g;
                    else
                    {
                        r=p;
                        h=0
                    }
                }
                p=c[e>>2]|0;
                m=c[p+12>>2]|0;
                if((m|0)==(c[p+16>>2]|0))p=Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0;
                else p=d[m>>0]|0;
                if((p&255)<<24>>24<=-1)break g;
                if(!(b[(c[H>>2]|0)+(p<<24>>24<<1)>>1]&8192))break g;
                p=c[e>>2]|0;
                m=p+12|0;
                g=c[m>>2]|0;
                if((g|0)==(c[p+16>>2]|0))p=Qb[c[(c[p>>2]|0)+40>>2]&63](p)|0;
                else
                {
                    c[m>>2]=g+1;
                    p=d[g>>0]|0
                }
                Qe($,p&255);
                p=r;
                g=h;
                V=28
            }
            while(0);
            G=G+1|0;
            if(G>>>0>=4)
            {
                V=202;
                break
            }
        }
        h:do if((V|0)==26)
        {
            c[k>>2]=c[k>>2]|4;
            g=0
        }
        else if((V|0)==82)
        {
            c[k>>2]=c[k>>2]|4;
            g=0
        }
        else if((V|0)==119)
        {
            c[k>>2]=c[k>>2]|4;
            g=0
        }
        else if((V|0)==167)
        {
            c[k>>2]=c[k>>2]|4;
            g=0
        }
        else if((V|0)==189)
        {
            c[k>>2]=c[k>>2]|4;
            g=0
        }
        else if((V|0)==200)
        {
            c[k>>2]=c[k>>2]|4;
            g=0
        }
        else if((V|0)==202)
        {
            i:do if(j)
            {
                q=j+1|0;
                r=j+8|0;
                s=j+4|0;
                m=1;
                j:while(1)
                    {
                    p=a[j>>0]|0;
                    if(!(p&1))p=(p&255)>>>1;
                    else p=c[s>>2]|0;
                    if(m>>>0>=p>>>0)break i;
                    p=c[e>>2]|0;
                    do if(p)
                    {
                        if((c[p+12>>2]|0)==(c[p+16>>2]|0))if((Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0)==-1)
                        {
                            c[e>>2]=0;
                            p=0;
                            break
                        }
                        else
                        {
                            p=c[e>>2]|0;
                            break
                        }
                    }
                    else p=0;
                    while(0);
                    p=(p|0)==0;
                    g=c[f>>2]|0;
                    do if(g)
                    {
                        if((c[g+12>>2]|0)==(c[g+16>>2]|0)?(Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0)==-1:0)
                        {
                            c[f>>2]=0;
                            V=218;
                            break
                        }
                        if(!p)break j
                    }
                    else V=218;
                    while(0);
                    if((V|0)==218?(V=0,p):0)break;
                    p=c[e>>2]|0;
                    g=c[p+12>>2]|0;
                    if((g|0)==(c[p+16>>2]|0))p=Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0;
                    else p=d[g>>0]|0;
                    if(!(a[j>>0]&1))g=q;
                    else g=c[r>>2]|0;
                    if((p&255)<<24>>24!=(a[g+m>>0]|0))break;
                    p=m+1|0;
                    g=c[e>>2]|0;
                    h=g+12|0;
                    m=c[h>>2]|0;
                    if((m|0)==(c[g+16>>2]|0))
                    {
                        Qb[c[(c[g>>2]|0)+40>>2]&63](g)|0;
                        m=p;
                        continue
                    }
                    else
                    {
                        c[h>>2]=m+1;
                        m=p;
                        continue
                    }
                }
                c[k>>2]=c[k>>2]|4;
                g=0;
                break h
            }
            while(0);
            p=c[ba>>2]|0;
            if((p|0)!=(t|0)?(c[W>>2]=0,Gj(Y,p,t,W),(c[W>>2]|0)!=0):0)
            {
                c[k>>2]=c[k>>2]|4;
                g=0
            }
            else g=1
        }
        while(0);
        Ke($);
        Ke(Z);
        Ke(_);
        Ke(aa);
        Ke(Y);
        p=c[ba>>2]|0;
        c[ba>>2]=0;
        if(p)Mb[c[X>>2]&127](p);
        i=ca;
        return g|0
    }
    function Dj(a)
    {
        a=a|0;
        return
    }
    function Ej(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;
        s=i;
        i=i+144|0;
        v=s+24|0;
        t=s+32|0;
        r=s+8|0;
        l=s+16|0;
        u=s+20|0;
        k=s+28|0;
        m=s;
        c[r>>2]=t;
        q=r+4|0;
        c[q>>2]=96;
        o=ff(g)|0;
        c[u>>2]=o;
        b=tk(u,6584)|0;
        a[k>>0]=0;
        n=c[e>>2]|0;
        c[m>>2]=n;
        g=c[g+4>>2]|0;
        c[v>>2]=c[m>>2];
        m=n;
        if(Cj(d,v,f,u,g,h,k,b,r,l,t+100|0)|0)
        {
            if(!(a[j>>0]&1))
            {
                a[j+1>>0]=0;
                a[j>>0]=0
            }
            else
            {
                a[c[j+8>>2]>>0]=0;
                c[j+4>>2]=0
            }
            if(a[k>>0]|0)Qe(j,Wb[c[(c[b>>2]|0)+28>>2]&15](b,45)|0);
            f=Wb[c[(c[b>>2]|0)+28>>2]&15](b,48)|0;
            b=c[r>>2]|0;
            k=c[l>>2]|0;
            g=k+-1|0;
            a:do if(b>>>0<g>>>0)do
                {
                if((a[b>>0]|0)!=f<<24>>24)break a;
                b=b+1|0
            }
            while(b>>>0<g>>>0);
            while(0);
            Hm(j,b,k)|0
        }
        b=c[d>>2]|0;
        do if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0))if((Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1)
            {
                c[d>>2]=0;
                b=0;
                break
            }
            else
            {
                b=c[d>>2]|0;
                break
            }
        }
        else b=0;
        while(0);
        b=(b|0)==0;
        do if(n)
        {
            if((c[m+12>>2]|0)==(c[m+16>>2]|0)?(Qb[c[(c[n>>2]|0)+36>>2]&63](m)|0)==-1:0)
            {
                c[e>>2]=0;
                p=21;
                break
            }
            if(!b)p=22
        }
        else p=21;
        while(0);
        if((p|0)==21?b:0)p=22;
        if((p|0)==22)c[h>>2]=c[h>>2]|2;
        g=c[d>>2]|0;
        Sn(o)|0;
        b=c[r>>2]|0;
        c[r>>2]=0;
        if(b)Mb[c[q>>2]&127](b);
        i=s;
        return g|0
    }
    function Fj(b,d,e,f,g,h,j,k,l,m)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;
        x=i;
        i=i+112|0;
        n=x+100|0;
        o=x+88|0;
        p=x+76|0;
        q=x+64|0;
        r=x+52|0;
        s=x+48|0;
        t=x+24|0;
        u=x+12|0;
        v=x;
        w=x+36|0;
        if(b)
        {
            b=tk(d,6168)|0;
            Nb[c[(c[b>>2]|0)+44>>2]&63](n,b);
            w=c[n>>2]|0;
            a[e>>0]=w;
            a[e+1>>0]=w>>8;
            a[e+2>>0]=w>>16;
            a[e+3>>0]=w>>24;
            Nb[c[(c[b>>2]|0)+32>>2]&63](o,b);
            if(!(a[l>>0]&1))
            {
                a[l+1>>0]=0;
                a[l>>0]=0
            }
            else
            {
                a[c[l+8>>2]>>0]=0;
                c[l+4>>2]=0
            }
            Pe(l,0);
            c[l>>2]=c[o>>2];
            c[l+4>>2]=c[o+4>>2];
            c[l+8>>2]=c[o+8>>2];
            c[o>>2]=0;
            c[o+4>>2]=0;
            c[o+8>>2]=0;
            Ke(o);
            Nb[c[(c[b>>2]|0)+28>>2]&63](p,b);
            if(!(a[k>>0]&1))
            {
                a[k+1>>0]=0;
                a[k>>0]=0
            }
            else
            {
                a[c[k+8>>2]>>0]=0;
                c[k+4>>2]=0
            }
            Pe(k,0);
            c[k>>2]=c[p>>2];
            c[k+4>>2]=c[p+4>>2];
            c[k+8>>2]=c[p+8>>2];
            c[p>>2]=0;
            c[p+4>>2]=0;
            c[p+8>>2]=0;
            Ke(p);
            a[f>>0]=Qb[c[(c[b>>2]|0)+12>>2]&63](b)|0;
            a[g>>0]=Qb[c[(c[b>>2]|0)+16>>2]&63](b)|0;
            Nb[c[(c[b>>2]|0)+20>>2]&63](q,b);
            if(!(a[h>>0]&1))
            {
                a[h+1>>0]=0;
                a[h>>0]=0
            }
            else
            {
                a[c[h+8>>2]>>0]=0;
                c[h+4>>2]=0
            }
            Pe(h,0);
            c[h>>2]=c[q>>2];
            c[h+4>>2]=c[q+4>>2];
            c[h+8>>2]=c[q+8>>2];
            c[q>>2]=0;
            c[q+4>>2]=0;
            c[q+8>>2]=0;
            Ke(q);
            Nb[c[(c[b>>2]|0)+24>>2]&63](r,b);
            if(!(a[j>>0]&1))
            {
                a[j+1>>0]=0;
                a[j>>0]=0
            }
            else
            {
                a[c[j+8>>2]>>0]=0;
                c[j+4>>2]=0
            }
            Pe(j,0);
            c[j>>2]=c[r>>2];
            c[j+4>>2]=c[r+4>>2];
            c[j+8>>2]=c[r+8>>2];
            c[r>>2]=0;
            c[r+4>>2]=0;
            c[r+8>>2]=0;
            Ke(r);
            b=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0
        }
        else
        {
            b=tk(d,6104)|0;
            Nb[c[(c[b>>2]|0)+44>>2]&63](s,b);
            r=c[s>>2]|0;
            a[e>>0]=r;
            a[e+1>>0]=r>>8;
            a[e+2>>0]=r>>16;
            a[e+3>>0]=r>>24;
            Nb[c[(c[b>>2]|0)+32>>2]&63](t,b);
            if(!(a[l>>0]&1))
            {
                a[l+1>>0]=0;
                a[l>>0]=0
            }
            else
            {
                a[c[l+8>>2]>>0]=0;
                c[l+4>>2]=0
            }
            Pe(l,0);
            c[l>>2]=c[t>>2];
            c[l+4>>2]=c[t+4>>2];
            c[l+8>>2]=c[t+8>>2];
            c[t>>2]=0;
            c[t+4>>2]=0;
            c[t+8>>2]=0;
            Ke(t);
            Nb[c[(c[b>>2]|0)+28>>2]&63](u,b);
            if(!(a[k>>0]&1))
            {
                a[k+1>>0]=0;
                a[k>>0]=0
            }
            else
            {
                a[c[k+8>>2]>>0]=0;
                c[k+4>>2]=0
            }
            Pe(k,0);
            c[k>>2]=c[u>>2];
            c[k+4>>2]=c[u+4>>2];
            c[k+8>>2]=c[u+8>>2];
            c[u>>2]=0;
            c[u+4>>2]=0;
            c[u+8>>2]=0;
            Ke(u);
            a[f>>0]=Qb[c[(c[b>>2]|0)+12>>2]&63](b)|0;
            a[g>>0]=Qb[c[(c[b>>2]|0)+16>>2]&63](b)|0;
            Nb[c[(c[b>>2]|0)+20>>2]&63](v,b);
            if(!(a[h>>0]&1))
            {
                a[h+1>>0]=0;
                a[h>>0]=0
            }
            else
            {
                a[c[h+8>>2]>>0]=0;
                c[h+4>>2]=0
            }
            Pe(h,0);
            c[h>>2]=c[v>>2];
            c[h+4>>2]=c[v+4>>2];
            c[h+8>>2]=c[v+8>>2];
            c[v>>2]=0;
            c[v+4>>2]=0;
            c[v+8>>2]=0;
            Ke(v);
            Nb[c[(c[b>>2]|0)+24>>2]&63](w,b);
            if(!(a[j>>0]&1))
            {
                a[j+1>>0]=0;
                a[j>>0]=0
            }
            else
            {
                a[c[j+8>>2]>>0]=0;
                c[j+4>>2]=0
            }
            Pe(j,0);
            c[j>>2]=c[w>>2];
            c[j+4>>2]=c[w+4>>2];
            c[j+8>>2]=c[w+8>>2];
            c[w>>2]=0;
            c[w+4>>2]=0;
            c[w+8>>2]=0;
            Ke(w);
            b=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0
        }
        c[m>>2]=b;
        i=x;
        return
    }
    function Gj(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,i=0,j=0;
        g=a[b>>0]|0;
        i=b+4|0;
        h=c[i>>2]|0;
        a:do if(((g&1)==0?(g&255)>>>1:h)|0)
        {
            if((d|0)!=(e|0))
            {
                g=e+-4|0;
                if(g>>>0>d>>>0)
                {
                    h=d;
                    do
                    {
                        j=c[h>>2]|0;
                        c[h>>2]=c[g>>2];
                        c[g>>2]=j;
                        h=h+4|0;
                        g=g+-4|0
                    }
                    while(h>>>0<g>>>0)
                }
                g=a[b>>0]|0;
                h=c[i>>2]|0
            }
            j=(g&1)==0;
            i=j?b+1|0:c[b+8>>2]|0;
            e=e+-4|0;
            b=i+(j?(g&255)>>>1:h)|0;
            h=a[i>>0]|0;
            g=h<<24>>24<1|h<<24>>24==127;
            b:do if(e>>>0>d>>>0)
            {
                while(1)
                {
                    if(!g?(h<<24>>24|0)!=(c[d>>2]|0):0)break;
                    i=(b-i|0)>1?i+1|0:i;
                    d=d+4|0;
                    h=a[i>>0]|0;
                    g=h<<24>>24<1|h<<24>>24==127;
                    if(d>>>0>=e>>>0)break b
                }
                c[f>>2]=4;
                break a
            }
            while(0);
            if(!g?((c[e>>2]|0)+-1|0)>>>0>=h<<24>>24>>>0:0)c[f>>2]=4
        }
        while(0);
        return
    }
    function Hj(a)
    {
        a=a|0;
        return
    }
    function Ij(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Jj(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0;
        F=i;
        i=i+576|0;
        w=F+16|0;
        z=F;
        v=F+64|0;
        E=F+56|0;
        x=F+12|0;
        B=F+8|0;
        k=F+564|0;
        t=F+4|0;
        y=F+464|0;
        c[E>>2]=v;
        D=E+4|0;
        c[D>>2]=96;
        c[B>>2]=ff(g)|0;
        b=tk(B,6576)|0;
        a[k>>0]=0;
        c[t>>2]=c[e>>2];
        u=c[g+4>>2]|0;
        c[w>>2]=c[t>>2];
        if(Kj(d,w,f,B,u,h,k,b,E,x,v+400|0)|0)
        {
            Ub[c[(c[b>>2]|0)+48>>2]&7](b,9904,9914,w)|0;
            f=c[x>>2]|0;
            t=c[E>>2]|0;
            b=f-t|0;
            if((b|0)>392)
            {
                b=$d((b>>2)+2|0)|0;
                if(!b)sd();
                else
                {
                    A=b;
                    l=b
                }
            }
            else
            {
                A=0;
                l=y
            }
            if(!(a[k>>0]|0))b=l;
            else
            {
                a[l>>0]=45;
                b=l+1|0
            }
            u=w+40|0;
            v=w;
            if(t>>>0<f>>>0)
            {
                g=w+4|0;
                l=g+4|0;
                m=l+4|0;
                n=m+4|0;
                o=n+4|0;
                p=o+4|0;
                q=p+4|0;
                r=q+4|0;
                s=r+4|0;
                f=t;
                do
                {
                    k=c[f>>2]|0;
                    if((c[w>>2]|0)!=(k|0))if((c[g>>2]|0)!=(k|0))if((c[l>>2]|0)!=(k|0))if((c[m>>2]|0)!=(k|0))if((c[n>>2]|0)!=(k|0))if((c[o>>2]|0)!=(k|0))if((c[p>>2]|0)!=(k|0))if((c[q>>2]|0)!=(k|0))if((c[r>>2]|0)==(k|0))k=r;
                    else k=(c[s>>2]|0)==(k|0)?s:u;
                    else k=q;
                    else k=p;
                    else k=o;
                    else k=n;
                    else k=m;
                    else k=l;
                    else k=g;
                    else k=w;
                    a[b>>0]=a[9904+(k-v>>2)>>0]|0;
                    f=f+4|0;
                    b=b+1|0
                }
                while(f>>>0<(c[x>>2]|0)>>>0)
            }
            a[b>>0]=0;
            c[z>>2]=j;
            yc(y,9896,z)|0;
            if(A)ae(A)
        }
        b=c[d>>2]|0;
        do if(b)
        {
            f=c[b+12>>2]|0;
            if((f|0)==(c[b+16>>2]|0))b=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else b=c[f>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                g=1;
                break
            }
            else
            {
                g=(c[d>>2]|0)==0;
                break
            }
        }
        else g=1;
        while(0);
        b=c[e>>2]|0;
        do if(b)
        {
            f=c[b+12>>2]|0;
            if((f|0)==(c[b+16>>2]|0))b=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else b=c[f>>2]|0;
            if((b|0)!=-1)if(g)break;
            else
            {
                C=30;
                break
            }
            else
            {
                c[e>>2]=0;
                C=28;
                break
            }
        }
        else C=28;
        while(0);
        if((C|0)==28?g:0)C=30;
        if((C|0)==30)c[h>>2]=c[h>>2]|2;
        f=c[d>>2]|0;
        Sn(c[B>>2]|0)|0;
        b=c[E>>2]|0;
        c[E>>2]=0;
        if(b)Mb[c[D>>2]&127](b);
        i=F;
        return f|0
    }
    function Kj(b,e,f,g,h,j,k,l,m,n,o)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        n=n|0;
        o=o|0;
        var p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0;
        Z=i;
        i=i+512|0;
        J=Z+496|0;
        s=Z+96|0;
        Y=Z+88|0;
        N=Z+80|0;
        M=Z+76|0;
        O=Z+500|0;
        K=Z+72|0;
        P=Z+68|0;
        T=Z+52|0;
        X=Z+40|0;
        V=Z+28|0;
        U=Z+16|0;
        W=Z+4|0;
        L=Z;
        R=Z+64|0;
        c[J>>2]=o;
        c[Y>>2]=s;
        S=Y+4|0;
        c[S>>2]=96;
        c[N>>2]=s;
        c[M>>2]=s+400;
        c[T>>2]=0;
        c[T+4>>2]=0;
        c[T+8>>2]=0;
        c[X>>2]=0;
        c[X+4>>2]=0;
        c[X+8>>2]=0;
        c[V>>2]=0;
        c[V+4>>2]=0;
        c[V+8>>2]=0;
        c[U>>2]=0;
        c[U+4>>2]=0;
        c[U+8>>2]=0;
        c[W>>2]=0;
        c[W+4>>2]=0;
        c[W+8>>2]=0;
        Mj(f,g,O,K,P,T,X,V,U,L);
        c[n>>2]=c[m>>2];
        F=V+4|0;
        G=U+4|0;
        H=U+8|0;
        I=V+8|0;
        x=(h&512|0)!=0;
        y=X+8|0;
        z=X+4|0;
        A=W+4|0;
        B=W+8|0;
        C=O+3|0;
        D=T+4|0;
        E=0;
        r=0;
        a:while(1)
            {
            o=c[b>>2]|0;
            do if(o)
            {
                f=c[o+12>>2]|0;
                if((f|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                else o=c[f>>2]|0;
                if((o|0)==-1)
                {
                    c[b>>2]=0;
                    g=1;
                    break
                }
                else
                {
                    g=(c[b>>2]|0)==0;
                    break
                }
            }
            else g=1;
            while(0);
            f=c[e>>2]|0;
            do if(f)
            {
                o=c[f+12>>2]|0;
                if((o|0)==(c[f+16>>2]|0))o=Qb[c[(c[f>>2]|0)+36>>2]&63](f)|0;
                else o=c[o>>2]|0;
                if((o|0)!=-1)if(g)
                {
                    w=f;
                    break
                }
                else
                {
                    Q=217;
                    break a
                }
                else
                {
                    c[e>>2]=0;
                    Q=15;
                    break
                }
            }
            else Q=15;
            while(0);
            if((Q|0)==15)
            {
                Q=0;
                if(g)
                {
                    Q=217;
                    break
                }
                else w=0
            }
            b:do switch(a[O+E>>0]|0)
                {
                case 2:
                    {
                        if(!(E>>>0<2|(r|0)!=0)?!(x|(E|0)==2&(a[C>>0]|0)!=0):0)
                        {
                            r=0;
                            break b
                        }
                        h=a[X>>0]|0;
                        g=c[y>>2]|0;
                        f=(h&1)==0?z:g;
                        o=f;
                        c:do if((E|0)!=0?(d[O+(E+-1)>>0]|0)<2:0)
                        {
                            v=(h&1)==0;
                            d:do if((f|0)!=((v?z:g)+((v?(h&255)>>>1:c[z>>2]|0)<<2)|0))
                            {
                                h=f;
                                while(1)
                                {
                                    if(!(Jb[c[(c[l>>2]|0)+12>>2]&31](l,8192,c[h>>2]|0)|0))break;
                                    h=h+4|0;
                                    o=h;
                                    f=a[X>>0]|0;
                                    g=c[y>>2]|0;
                                    v=(f&1)==0;
                                    if((h|0)==((v?z:g)+((v?(f&255)>>>1:c[z>>2]|0)<<2)|0))
                                    {
                                        h=f;
                                        break d
                                    }
                                }
                                h=a[X>>0]|0;
                                g=c[y>>2]|0
                            }
                            while(0);
                            t=(h&1)==0?z:g;
                            f=t;
                            p=o-f>>2;
                            u=a[W>>0]|0;
                            q=(u&1)==0;
                            u=q?(u&255)>>>1:c[A>>2]|0;
                            if(u>>>0>=p>>>0)
                            {
                                q=q?A:c[B>>2]|0;
                                v=q+(u<<2)|0;
                                if(!p)f=o;
                                else
                                {
                                    q=q+(u-p<<2)|0;
                                    while(1)
                                    {
                                        if((c[q>>2]|0)!=(c[t>>2]|0))break c;
                                        q=q+4|0;
                                        if((q|0)==(v|0))
                                        {
                                            f=o;
                                            break
                                        }
                                        else t=t+4|0
                                    }
                                }
                            }
                        }
                        else f=o;
                        while(0);
                        o=(h&1)==0;
                        o=(o?z:g)+((o?(h&255)>>>1:c[z>>2]|0)<<2)|0;
                        e:do if((f|0)!=(o|0))
                        {
                            o=w;
                            h=w;
                            q=f;
                            while(1)
                            {
                                f=c[b>>2]|0;
                                do if(f)
                                {
                                    g=c[f+12>>2]|0;
                                    if((g|0)==(c[f+16>>2]|0))f=Qb[c[(c[f>>2]|0)+36>>2]&63](f)|0;
                                    else f=c[g>>2]|0;
                                    if((f|0)==-1)
                                    {
                                        c[b>>2]=0;
                                        g=1;
                                        break
                                    }
                                    else
                                    {
                                        g=(c[b>>2]|0)==0;
                                        break
                                    }
                                }
                                else g=1;
                                while(0);
                                do if(h)
                                {
                                    f=c[h+12>>2]|0;
                                    if((f|0)==(c[h+16>>2]|0))f=Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0;
                                    else f=c[f>>2]|0;
                                    if((f|0)!=-1)if(g^(o|0)==0)
                                    {
                                        t=o;
                                        h=o;
                                        break
                                    }
                                    else
                                    {
                                        o=q;
                                        break e
                                    }
                                    else
                                    {
                                        c[e>>2]=0;
                                        o=0;
                                        Q=114;
                                        break
                                    }
                                }
                                else Q=114;
                                while(0);
                                if((Q|0)==114)
                                {
                                    Q=0;
                                    if(g)
                                    {
                                        o=q;
                                        break e
                                    }
                                    else
                                    {
                                        t=o;
                                        h=0
                                    }
                                }
                                o=c[b>>2]|0;
                                f=c[o+12>>2]|0;
                                if((f|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                                else o=c[f>>2]|0;
                                if((o|0)!=(c[q>>2]|0))
                                {
                                    o=q;
                                    break e
                                }
                                o=c[b>>2]|0;
                                f=o+12|0;
                                g=c[f>>2]|0;
                                if((g|0)==(c[o+16>>2]|0))Qb[c[(c[o>>2]|0)+40>>2]&63](o)|0;
                                else c[f>>2]=g+4;
                                q=q+4|0;
                                o=a[X>>0]|0;
                                w=(o&1)==0;
                                o=(w?z:c[y>>2]|0)+((w?(o&255)>>>1:c[z>>2]|0)<<2)|0;
                                if((q|0)==(o|0))break;
                                else o=t
                            }
                        }
                        while(0);
                        if(x?(w=a[X>>0]|0,v=(w&1)==0,(o|0)!=((v?z:c[y>>2]|0)+((v?(w&255)>>>1:c[z>>2]|0)<<2)|0)):0)
                        {
                            Q=126;
                            break a
                        }
                        break
                    }
                case 3:
                    {
                        q=a[V>>0]|0;
                        o=(q&1)==0?(q&255)>>>1:c[F>>2]|0;
                        g=a[U>>0]|0;
                        g=(g&1)==0?(g&255)>>>1:c[G>>2]|0;
                        if((o|0)!=(0-g|0))
                        {
                            h=(o|0)==0;
                            p=c[b>>2]|0;
                            t=c[p+12>>2]|0;
                            o=c[p+16>>2]|0;
                            f=(t|0)==(o|0);
                            if(h|(g|0)==0)
                            {
                                if(f)o=Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0;
                                else o=c[t>>2]|0;
                                if(h)
                                {
                                    if((o|0)!=(c[((a[U>>0]&1)==0?G:c[H>>2]|0)>>2]|0))break b;
                                    o=c[b>>2]|0;
                                    f=o+12|0;
                                    g=c[f>>2]|0;
                                    if((g|0)==(c[o+16>>2]|0))Qb[c[(c[o>>2]|0)+40>>2]&63](o)|0;
                                    else c[f>>2]=g+4;
                                    a[k>>0]=1;
                                    w=a[U>>0]|0;
                                    r=((w&1)==0?(w&255)>>>1:c[G>>2]|0)>>>0>1?U:r;
                                    break b
                                }
                                if((o|0)!=(c[((a[V>>0]&1)==0?F:c[I>>2]|0)>>2]|0))
                                {
                                    a[k>>0]=1;
                                    break b
                                }
                                o=c[b>>2]|0;
                                f=o+12|0;
                                g=c[f>>2]|0;
                                if((g|0)==(c[o+16>>2]|0))Qb[c[(c[o>>2]|0)+40>>2]&63](o)|0;
                                else c[f>>2]=g+4;
                                w=a[V>>0]|0;
                                r=((w&1)==0?(w&255)>>>1:c[F>>2]|0)>>>0>1?V:r;
                                break b
                            }
                            if(f)
                            {
                                h=Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0;
                                o=c[b>>2]|0;
                                q=a[V>>0]|0;
                                p=o;
                                g=c[o+12>>2]|0;
                                o=c[o+16>>2]|0
                            }
                            else
                            {
                                h=c[t>>2]|0;
                                g=t
                            }
                            f=p+12|0;
                            o=(g|0)==(o|0);
                            if((h|0)==(c[((q&1)==0?F:c[I>>2]|0)>>2]|0))
                            {
                                if(o)Qb[c[(c[p>>2]|0)+40>>2]&63](p)|0;
                                else c[f>>2]=g+4;
                                w=a[V>>0]|0;
                                r=((w&1)==0?(w&255)>>>1:c[F>>2]|0)>>>0>1?V:r;
                                break b
                            }
                            if(o)o=Qb[c[(c[p>>2]|0)+36>>2]&63](p)|0;
                            else o=c[g>>2]|0;
                            if((o|0)!=(c[((a[U>>0]&1)==0?G:c[H>>2]|0)>>2]|0))
                            {
                                Q=86;
                                break a
                            }
                            o=c[b>>2]|0;
                            f=o+12|0;
                            g=c[f>>2]|0;
                            if((g|0)==(c[o+16>>2]|0))Qb[c[(c[o>>2]|0)+40>>2]&63](o)|0;
                            else c[f>>2]=g+4;
                            a[k>>0]=1;
                            w=a[U>>0]|0;
                            r=((w&1)==0?(w&255)>>>1:c[G>>2]|0)>>>0>1?U:r
                        }
                        break
                    }
                case 1:
                    {
                        if((E|0)!=3)
                        {
                            o=c[b>>2]|0;
                            f=c[o+12>>2]|0;
                            if((f|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                            else o=c[f>>2]|0;
                            if(!(Jb[c[(c[l>>2]|0)+12>>2]&31](l,8192,o)|0))
                            {
                                Q=28;
                                break a
                            }
                            o=c[b>>2]|0;
                            f=o+12|0;
                            g=c[f>>2]|0;
                            if((g|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+40>>2]&63](o)|0;
                            else
                            {
                                c[f>>2]=g+4;
                                o=c[g>>2]|0
                            }
                            Ze(W,o);
                            o=w;
                            h=w;
                            Q=30
                        }
                        break
                    }
                case 4:
                    {
                        p=c[P>>2]|0;
                        h=w;
                        t=w;
                        o=0;
                        f:while(1)
                            {
                            f=c[b>>2]|0;
                            do if(f)
                            {
                                g=c[f+12>>2]|0;
                                if((g|0)==(c[f+16>>2]|0))f=Qb[c[(c[f>>2]|0)+36>>2]&63](f)|0;
                                else f=c[g>>2]|0;
                                if((f|0)==-1)
                                {
                                    c[b>>2]=0;
                                    g=1;
                                    break
                                }
                                else
                                {
                                    g=(c[b>>2]|0)==0;
                                    break
                                }
                            }
                            else g=1;
                            while(0);
                            do if(t)
                            {
                                f=c[t+12>>2]|0;
                                if((f|0)==(c[t+16>>2]|0))f=Qb[c[(c[t>>2]|0)+36>>2]&63](t)|0;
                                else f=c[f>>2]|0;
                                if((f|0)!=-1)if(g^(h|0)==0)
                                {
                                    f=h;
                                    t=h;
                                    break
                                }
                                else break f;
                                else
                                {
                                    c[e>>2]=0;
                                    f=0;
                                    Q=140;
                                    break
                                }
                            }
                            else
                            {
                                f=h;
                                Q=140
                            }
                            while(0);
                            if((Q|0)==140)
                            {
                                Q=0;
                                if(g)
                                {
                                    h=f;
                                    break
                                }
                                else t=0
                            }
                            g=c[b>>2]|0;
                            h=c[g+12>>2]|0;
                            if((h|0)==(c[g+16>>2]|0))h=Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0;
                            else h=c[h>>2]|0;
                            if(Jb[c[(c[l>>2]|0)+12>>2]&31](l,2048,h)|0)
                            {
                                g=c[n>>2]|0;
                                if((g|0)==(c[J>>2]|0))
                                {
                                    Im(m,n,J);
                                    g=c[n>>2]|0
                                }
                                c[n>>2]=g+4;
                                c[g>>2]=h;
                                o=o+1|0
                            }
                            else
                            {
                                w=a[T>>0]|0;
                                if(!((h|0)==(p|0)&((o|0)!=0?(((w&1)==0?(w&255)>>>1:c[D>>2]|0)|0)!=0:0)))
                                {
                                    h=f;
                                    break
                                }
                                if((s|0)==(c[M>>2]|0))
                                {
                                    Gm(Y,N,M);
                                    s=c[N>>2]|0
                                }
                                w=s+4|0;
                                c[N>>2]=w;
                                c[s>>2]=o;
                                s=w;
                                o=0
                            }
                            g=c[b>>2]|0;
                            h=g+12|0;
                            q=c[h>>2]|0;
                            if((q|0)==(c[g+16>>2]|0))
                            {
                                Qb[c[(c[g>>2]|0)+40>>2]&63](g)|0;
                                h=f;
                                continue
                            }
                            else
                            {
                                c[h>>2]=q+4;
                                h=f;
                                continue
                            }
                        }
                        if((o|0)!=0?(c[Y>>2]|0)!=(s|0):0)
                        {
                            if((s|0)==(c[M>>2]|0))
                            {
                                Gm(Y,N,M);
                                s=c[N>>2]|0
                            }
                            w=s+4|0;
                            c[N>>2]=w;
                            c[s>>2]=o;
                            s=w
                        }
                        q=c[L>>2]|0;
                        if((q|0)>0)
                        {
                            o=c[b>>2]|0;
                            do if(o)
                            {
                                f=c[o+12>>2]|0;
                                if((f|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                                else o=c[f>>2]|0;
                                if((o|0)==-1)
                                {
                                    c[b>>2]=0;
                                    f=1;
                                    break
                                }
                                else
                                {
                                    f=(c[b>>2]|0)==0;
                                    break
                                }
                            }
                            else f=1;
                            while(0);
                            do if(h)
                            {
                                o=c[h+12>>2]|0;
                                if((o|0)==(c[h+16>>2]|0))o=Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0;
                                else o=c[o>>2]|0;
                                if((o|0)!=-1)if(f)break;
                                else
                                {
                                    Q=180;
                                    break a
                                }
                                else
                                {
                                    c[e>>2]=0;
                                    Q=174;
                                    break
                                }
                            }
                            else Q=174;
                            while(0);
                            if((Q|0)==174)
                            {
                                Q=0;
                                if(f)
                                {
                                    Q=180;
                                    break a
                                }
                                else h=0
                            }
                            o=c[b>>2]|0;
                            f=c[o+12>>2]|0;
                            if((f|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                            else o=c[f>>2]|0;
                            if((o|0)!=(c[K>>2]|0))
                            {
                                Q=180;
                                break a
                            }
                            o=c[b>>2]|0;
                            f=o+12|0;
                            g=c[f>>2]|0;
                            if((g|0)==(c[o+16>>2]|0))Qb[c[(c[o>>2]|0)+40>>2]&63](o)|0;
                            else c[f>>2]=g+4;
                            if((q|0)>0)
                            {
                                t=h;
                                g=h;
                                u=q;
                                while(1)
                                {
                                    o=c[b>>2]|0;
                                    do if(o)
                                    {
                                        f=c[o+12>>2]|0;
                                        if((f|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                                        else o=c[f>>2]|0;
                                        if((o|0)==-1)
                                        {
                                            c[b>>2]=0;
                                            f=1;
                                            break
                                        }
                                        else
                                        {
                                            f=(c[b>>2]|0)==0;
                                            break
                                        }
                                    }
                                    else f=1;
                                    while(0);
                                    do if(g)
                                    {
                                        o=c[g+12>>2]|0;
                                        if((o|0)==(c[g+16>>2]|0))o=Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0;
                                        else o=c[o>>2]|0;
                                        if((o|0)!=-1)if(f^(t|0)==0)
                                        {
                                            o=t;
                                            p=t;
                                            break
                                        }
                                        else
                                        {
                                            Q=204;
                                            break a
                                        }
                                        else
                                        {
                                            c[e>>2]=0;
                                            o=0;
                                            Q=198;
                                            break
                                        }
                                    }
                                    else
                                    {
                                        o=t;
                                        Q=198
                                    }
                                    while(0);
                                    if((Q|0)==198)
                                    {
                                        Q=0;
                                        if(f)
                                        {
                                            Q=204;
                                            break a
                                        }
                                        else p=0
                                    }
                                    f=c[b>>2]|0;
                                    g=c[f+12>>2]|0;
                                    if((g|0)==(c[f+16>>2]|0))f=Qb[c[(c[f>>2]|0)+36>>2]&63](f)|0;
                                    else f=c[g>>2]|0;
                                    if(!(Jb[c[(c[l>>2]|0)+12>>2]&31](l,2048,f)|0))
                                    {
                                        Q=204;
                                        break a
                                    }
                                    if((c[n>>2]|0)==(c[J>>2]|0))Im(m,n,J);
                                    f=c[b>>2]|0;
                                    g=c[f+12>>2]|0;
                                    if((g|0)==(c[f+16>>2]|0))f=Qb[c[(c[f>>2]|0)+36>>2]&63](f)|0;
                                    else f=c[g>>2]|0;
                                    g=c[n>>2]|0;
                                    c[n>>2]=g+4;
                                    c[g>>2]=f;
                                    f=u;
                                    u=u+-1|0;
                                    c[L>>2]=u;
                                    g=c[b>>2]|0;
                                    h=g+12|0;
                                    q=c[h>>2]|0;
                                    if((q|0)==(c[g+16>>2]|0))Qb[c[(c[g>>2]|0)+40>>2]&63](g)|0;
                                    else c[h>>2]=q+4;
                                    if((f|0)<=1)break;
                                    else
                                    {
                                        t=o;
                                        g=p
                                    }
                                }
                            }
                        }
                        if((c[n>>2]|0)==(c[m>>2]|0))
                        {
                            Q=215;
                            break a
                        }
                        break
                    }
                case 0:
                    {
                        if((E|0)!=3)
                        {
                            o=w;
                            h=w;
                            Q=30
                        }
                        break
                    }
                default:
                    {
                    }
            }
            while(0);
            g:do if((Q|0)==30)while(1)
                {
                Q=0;
                f=c[b>>2]|0;
                do if(f)
                {
                    g=c[f+12>>2]|0;
                    if((g|0)==(c[f+16>>2]|0))f=Qb[c[(c[f>>2]|0)+36>>2]&63](f)|0;
                    else f=c[g>>2]|0;
                    if((f|0)==-1)
                    {
                        c[b>>2]=0;
                        g=1;
                        break
                    }
                    else
                    {
                        g=(c[b>>2]|0)==0;
                        break
                    }
                }
                else g=1;
                while(0);
                do if(h)
                {
                    f=c[h+12>>2]|0;
                    if((f|0)==(c[h+16>>2]|0))f=Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0;
                    else f=c[f>>2]|0;
                    if((f|0)!=-1)if(g^(o|0)==0)
                    {
                        q=o;
                        h=o;
                        break
                    }
                    else break g;
                    else
                    {
                        c[e>>2]=0;
                        o=0;
                        Q=43;
                        break
                    }
                }
                else Q=43;
                while(0);
                if((Q|0)==43)
                {
                    Q=0;
                    if(g)break g;
                    else
                    {
                        q=o;
                        h=0
                    }
                }
                o=c[b>>2]|0;
                f=c[o+12>>2]|0;
                if((f|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                else o=c[f>>2]|0;
                if(!(Jb[c[(c[l>>2]|0)+12>>2]&31](l,8192,o)|0))break g;
                o=c[b>>2]|0;
                f=o+12|0;
                g=c[f>>2]|0;
                if((g|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+40>>2]&63](o)|0;
                else
                {
                    c[f>>2]=g+4;
                    o=c[g>>2]|0
                }
                Ze(W,o);
                o=q;
                Q=30
            }
            while(0);
            E=E+1|0;
            if(E>>>0>=4)
            {
                Q=217;
                break
            }
        }
        h:do if((Q|0)==28)
        {
            c[j>>2]=c[j>>2]|4;
            g=0
        }
        else if((Q|0)==86)
        {
            c[j>>2]=c[j>>2]|4;
            g=0
        }
        else if((Q|0)==126)
        {
            c[j>>2]=c[j>>2]|4;
            g=0
        }
        else if((Q|0)==180)
        {
            c[j>>2]=c[j>>2]|4;
            g=0
        }
        else if((Q|0)==204)
        {
            c[j>>2]=c[j>>2]|4;
            g=0
        }
        else if((Q|0)==215)
        {
            c[j>>2]=c[j>>2]|4;
            g=0
        }
        else if((Q|0)==217)
        {
            i:do if(r)
            {
                p=r+4|0;
                q=r+8|0;
                h=1;
                j:while(1)
                    {
                    o=a[r>>0]|0;
                    if(!(o&1))o=(o&255)>>>1;
                    else o=c[p>>2]|0;
                    if(h>>>0>=o>>>0)break i;
                    o=c[b>>2]|0;
                    do if(o)
                    {
                        g=c[o+12>>2]|0;
                        if((g|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                        else o=c[g>>2]|0;
                        if((o|0)==-1)
                        {
                            c[b>>2]=0;
                            f=1;
                            break
                        }
                        else
                        {
                            f=(c[b>>2]|0)==0;
                            break
                        }
                    }
                    else f=1;
                    while(0);
                    o=c[e>>2]|0;
                    do if(o)
                    {
                        g=c[o+12>>2]|0;
                        if((g|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                        else o=c[g>>2]|0;
                        if((o|0)!=-1)if(f)break;
                        else break j;
                        else
                        {
                            c[e>>2]=0;
                            Q=236;
                            break
                        }
                    }
                    else Q=236;
                    while(0);
                    if((Q|0)==236?(Q=0,f):0)break;
                    o=c[b>>2]|0;
                    g=c[o+12>>2]|0;
                    if((g|0)==(c[o+16>>2]|0))o=Qb[c[(c[o>>2]|0)+36>>2]&63](o)|0;
                    else o=c[g>>2]|0;
                    if(!(a[r>>0]&1))g=p;
                    else g=c[q>>2]|0;
                    if((o|0)!=(c[g+(h<<2)>>2]|0))break;
                    o=h+1|0;
                    g=c[b>>2]|0;
                    h=g+12|0;
                    f=c[h>>2]|0;
                    if((f|0)==(c[g+16>>2]|0))
                    {
                        Qb[c[(c[g>>2]|0)+40>>2]&63](g)|0;
                        h=o;
                        continue
                    }
                    else
                    {
                        c[h>>2]=f+4;
                        h=o;
                        continue
                    }
                }
                c[j>>2]=c[j>>2]|4;
                g=0;
                break h
            }
            while(0);
            o=c[Y>>2]|0;
            if((o|0)!=(s|0)?(c[R>>2]=0,Gj(T,o,s,R),(c[R>>2]|0)!=0):0)
            {
                c[j>>2]=c[j>>2]|4;
                g=0
            }
            else g=1
        }
        while(0);
        Ve(W);
        Ve(U);
        Ve(V);
        Ve(X);
        Ke(T);
        o=c[Y>>2]|0;
        c[Y>>2]=0;
        if(o)Mb[c[S>>2]&127](o);
        i=Z;
        return g|0
    }
    function Lj(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;
        s=i;
        i=i+432|0;
        v=s+420|0;
        t=s;
        r=s+400|0;
        l=s+408|0;
        u=s+412|0;
        k=s+424|0;
        m=s+416|0;
        c[r>>2]=t;
        q=r+4|0;
        c[q>>2]=96;
        o=ff(g)|0;
        c[u>>2]=o;
        b=tk(u,6576)|0;
        a[k>>0]=0;
        n=c[e>>2]|0;
        c[m>>2]=n;
        g=c[g+4>>2]|0;
        c[v>>2]=c[m>>2];
        m=n;
        if(Kj(d,v,f,u,g,h,k,b,r,l,t+400|0)|0)
        {
            if(!(a[j>>0]&1))a[j>>0]=0;
            else c[c[j+8>>2]>>2]=0;
            c[j+4>>2]=0;
            if(a[k>>0]|0)Ze(j,Wb[c[(c[b>>2]|0)+44>>2]&15](b,45)|0);
            f=Wb[c[(c[b>>2]|0)+44>>2]&15](b,48)|0;
            b=c[r>>2]|0;
            k=c[l>>2]|0;
            g=k+-4|0;
            a:do if(b>>>0<g>>>0)do
                {
                if((c[b>>2]|0)!=(f|0))break a;
                b=b+4|0
            }
            while(b>>>0<g>>>0);
            while(0);
            Jm(j,b,k)|0
        }
        b=c[d>>2]|0;
        do if(b)
        {
            g=c[b+12>>2]|0;
            if((g|0)==(c[b+16>>2]|0))b=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else b=c[g>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                g=1;
                break
            }
            else
            {
                g=(c[d>>2]|0)==0;
                break
            }
        }
        else g=1;
        while(0);
        do if(n)
        {
            b=c[m+12>>2]|0;
            if((b|0)==(c[m+16>>2]|0))b=Qb[c[(c[n>>2]|0)+36>>2]&63](m)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(g)break;
            else
            {
                p=26;
                break
            }
            else
            {
                c[e>>2]=0;
                p=24;
                break
            }
        }
        else p=24;
        while(0);
        if((p|0)==24?g:0)p=26;
        if((p|0)==26)c[h>>2]=c[h>>2]|2;
        g=c[d>>2]|0;
        Sn(o)|0;
        b=c[r>>2]|0;
        c[r>>2]=0;
        if(b)Mb[c[q>>2]&127](b);
        i=s;
        return g|0
    }
    function Mj(b,d,e,f,g,h,j,k,l,m)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;
        x=i;
        i=i+112|0;
        n=x+100|0;
        o=x+88|0;
        p=x+76|0;
        q=x+64|0;
        r=x+52|0;
        s=x+48|0;
        t=x+24|0;
        u=x+12|0;
        v=x;
        w=x+36|0;
        if(b)
        {
            b=tk(d,6296)|0;
            Nb[c[(c[b>>2]|0)+44>>2]&63](n,b);
            w=c[n>>2]|0;
            a[e>>0]=w;
            a[e+1>>0]=w>>8;
            a[e+2>>0]=w>>16;
            a[e+3>>0]=w>>24;
            Nb[c[(c[b>>2]|0)+32>>2]&63](o,b);
            if(!(a[l>>0]&1))a[l>>0]=0;
            else c[c[l+8>>2]>>2]=0;
            c[l+4>>2]=0;
            Ye(l,0);
            c[l>>2]=c[o>>2];
            c[l+4>>2]=c[o+4>>2];
            c[l+8>>2]=c[o+8>>2];
            c[o>>2]=0;
            c[o+4>>2]=0;
            c[o+8>>2]=0;
            Ve(o);
            Nb[c[(c[b>>2]|0)+28>>2]&63](p,b);
            if(!(a[k>>0]&1))a[k>>0]=0;
            else c[c[k+8>>2]>>2]=0;
            c[k+4>>2]=0;
            Ye(k,0);
            c[k>>2]=c[p>>2];
            c[k+4>>2]=c[p+4>>2];
            c[k+8>>2]=c[p+8>>2];
            c[p>>2]=0;
            c[p+4>>2]=0;
            c[p+8>>2]=0;
            Ve(p);
            c[f>>2]=Qb[c[(c[b>>2]|0)+12>>2]&63](b)|0;
            c[g>>2]=Qb[c[(c[b>>2]|0)+16>>2]&63](b)|0;
            Nb[c[(c[b>>2]|0)+20>>2]&63](q,b);
            if(!(a[h>>0]&1))
            {
                a[h+1>>0]=0;
                a[h>>0]=0
            }
            else
            {
                a[c[h+8>>2]>>0]=0;
                c[h+4>>2]=0
            }
            Pe(h,0);
            c[h>>2]=c[q>>2];
            c[h+4>>2]=c[q+4>>2];
            c[h+8>>2]=c[q+8>>2];
            c[q>>2]=0;
            c[q+4>>2]=0;
            c[q+8>>2]=0;
            Ke(q);
            Nb[c[(c[b>>2]|0)+24>>2]&63](r,b);
            if(!(a[j>>0]&1))a[j>>0]=0;
            else c[c[j+8>>2]>>2]=0;
            c[j+4>>2]=0;
            Ye(j,0);
            c[j>>2]=c[r>>2];
            c[j+4>>2]=c[r+4>>2];
            c[j+8>>2]=c[r+8>>2];
            c[r>>2]=0;
            c[r+4>>2]=0;
            c[r+8>>2]=0;
            Ve(r);
            b=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0
        }
        else
        {
            b=tk(d,6232)|0;
            Nb[c[(c[b>>2]|0)+44>>2]&63](s,b);
            r=c[s>>2]|0;
            a[e>>0]=r;
            a[e+1>>0]=r>>8;
            a[e+2>>0]=r>>16;
            a[e+3>>0]=r>>24;
            Nb[c[(c[b>>2]|0)+32>>2]&63](t,b);
            if(!(a[l>>0]&1))a[l>>0]=0;
            else c[c[l+8>>2]>>2]=0;
            c[l+4>>2]=0;
            Ye(l,0);
            c[l>>2]=c[t>>2];
            c[l+4>>2]=c[t+4>>2];
            c[l+8>>2]=c[t+8>>2];
            c[t>>2]=0;
            c[t+4>>2]=0;
            c[t+8>>2]=0;
            Ve(t);
            Nb[c[(c[b>>2]|0)+28>>2]&63](u,b);
            if(!(a[k>>0]&1))a[k>>0]=0;
            else c[c[k+8>>2]>>2]=0;
            c[k+4>>2]=0;
            Ye(k,0);
            c[k>>2]=c[u>>2];
            c[k+4>>2]=c[u+4>>2];
            c[k+8>>2]=c[u+8>>2];
            c[u>>2]=0;
            c[u+4>>2]=0;
            c[u+8>>2]=0;
            Ve(u);
            c[f>>2]=Qb[c[(c[b>>2]|0)+12>>2]&63](b)|0;
            c[g>>2]=Qb[c[(c[b>>2]|0)+16>>2]&63](b)|0;
            Nb[c[(c[b>>2]|0)+20>>2]&63](v,b);
            if(!(a[h>>0]&1))
            {
                a[h+1>>0]=0;
                a[h>>0]=0
            }
            else
            {
                a[c[h+8>>2]>>0]=0;
                c[h+4>>2]=0
            }
            Pe(h,0);
            c[h>>2]=c[v>>2];
            c[h+4>>2]=c[v+4>>2];
            c[h+8>>2]=c[v+8>>2];
            c[v>>2]=0;
            c[v+4>>2]=0;
            c[v+8>>2]=0;
            Ke(v);
            Nb[c[(c[b>>2]|0)+24>>2]&63](w,b);
            if(!(a[j>>0]&1))a[j>>0]=0;
            else c[c[j+8>>2]>>2]=0;
            c[j+4>>2]=0;
            Ye(j,0);
            c[j>>2]=c[w>>2];
            c[j+4>>2]=c[w+4>>2];
            c[j+8>>2]=c[w+8>>2];
            c[w>>2]=0;
            c[w+4>>2]=0;
            c[w+8>>2]=0;
            Ve(w);
            b=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0
        }
        c[m>>2]=b;
        i=x;
        return
    }
    function Nj(a)
    {
        a=a|0;
        return
    }
    function Oj(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Pj(b,d,e,f,g,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        j=+j;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0;
        F=i;
        i=i+384|0;
        q=F+8|0;
        l=F;
        b=F+280|0;
        m=F+72|0;
        k=F+76|0;
        y=F+68|0;
        v=F+276|0;
        s=F+381|0;
        w=F+380|0;
        C=F+48|0;
        E=F+36|0;
        D=F+24|0;
        o=F+20|0;
        p=F+176|0;
        u=F+16|0;
        t=F+60|0;
        r=F+64|0;
        c[m>>2]=b;
        h[q>>3]=j;
        b=Rd(b,100,9920,q)|0;
        if(b>>>0>99)
        {
            b=Jg()|0;
            h[l>>3]=j;
            b=Bm(m,b,9920,l)|0;
            k=c[m>>2]|0;
            if(!k)sd();
            l=$d(b)|0;
            if(!l)sd();
            else
            {
                G=l;
                H=k;
                x=l;
                A=b
            }
        }
        else
        {
            G=0;
            H=0;
            x=k;
            A=b
        }
        b=ff(f)|0;
        c[y>>2]=b;
        n=tk(y,6584)|0;
        l=c[m>>2]|0;
        Ub[c[(c[n>>2]|0)+32>>2]&7](n,l,l+A|0,x)|0;
        if(!A)m=0;
        else m=(a[c[m>>2]>>0]|0)==45;
        c[C>>2]=0;
        c[C+4>>2]=0;
        c[C+8>>2]=0;
        c[E>>2]=0;
        c[E+4>>2]=0;
        c[E+8>>2]=0;
        c[D>>2]=0;
        c[D+4>>2]=0;
        c[D+8>>2]=0;
        Qj(e,m,y,v,s,w,C,E,D,o);
        l=c[o>>2]|0;
        if((A|0)>(l|0))
        {
            o=a[D>>0]|0;
            k=a[E>>0]|0;
            k=(A-l<<1|1)+l+((o&1)==0?(o&255)>>>1:c[D+4>>2]|0)+((k&1)==0?(k&255)>>>1:c[E+4>>2]|0)|0
        }
        else
        {
            o=a[D>>0]|0;
            k=a[E>>0]|0;
            k=l+2+((o&1)==0?(o&255)>>>1:c[D+4>>2]|0)+((k&1)==0?(k&255)>>>1:c[E+4>>2]|0)|0
        }
        if(k>>>0>100)
        {
            k=$d(k)|0;
            if(!k)sd();
            else
            {
                B=k;
                z=k
            }
        }
        else
        {
            B=0;
            z=p
        }
        Rj(z,u,t,c[f+4>>2]|0,x,x+A|0,n,m,v,a[s>>0]|0,a[w>>0]|0,C,E,D,l);
        c[r>>2]=c[d>>2];
        d=c[u>>2]|0;
        k=c[t>>2]|0;
        c[q>>2]=c[r>>2];
        k=kc(q,z,d,k,f,g)|0;
        if(B)
        {
            ae(B);
            b=c[y>>2]|0
        }
        Ke(D);
        Ke(E);
        Ke(C);
        Sn(b)|0;
        if(G)ae(G);
        if(H)ae(H);
        i=F;
        return k|0
    }
    function Qj(b,d,e,f,g,h,j,k,l,m)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0;
        z=i;
        i=i+112|0;
        n=z+108|0;
        o=z+96|0;
        p=z+92|0;
        q=z+80|0;
        x=z+68|0;
        y=z+56|0;
        r=z+52|0;
        s=z+28|0;
        t=z+24|0;
        u=z+12|0;
        v=z;
        w=z+40|0;
        if(b)
        {
            e=tk(e,6168)|0;
            b=c[e>>2]|0;
            if(d)
            {
                Nb[c[b+44>>2]&63](n,e);
                d=c[n>>2]|0;
                a[f>>0]=d;
                a[f+1>>0]=d>>8;
                a[f+2>>0]=d>>16;
                a[f+3>>0]=d>>24;
                Nb[c[(c[e>>2]|0)+32>>2]&63](o,e);
                if(!(a[l>>0]&1))
                {
                    a[l+1>>0]=0;
                    a[l>>0]=0
                }
                else
                {
                    a[c[l+8>>2]>>0]=0;
                    c[l+4>>2]=0
                }
                Pe(l,0);
                c[l>>2]=c[o>>2];
                c[l+4>>2]=c[o+4>>2];
                c[l+8>>2]=c[o+8>>2];
                c[o>>2]=0;
                c[o+4>>2]=0;
                c[o+8>>2]=0;
                Ke(o);
                b=e
            }
            else
            {
                Nb[c[b+40>>2]&63](p,e);
                d=c[p>>2]|0;
                a[f>>0]=d;
                a[f+1>>0]=d>>8;
                a[f+2>>0]=d>>16;
                a[f+3>>0]=d>>24;
                Nb[c[(c[e>>2]|0)+28>>2]&63](q,e);
                if(!(a[l>>0]&1))
                {
                    a[l+1>>0]=0;
                    a[l>>0]=0
                }
                else
                {
                    a[c[l+8>>2]>>0]=0;
                    c[l+4>>2]=0
                }
                Pe(l,0);
                c[l>>2]=c[q>>2];
                c[l+4>>2]=c[q+4>>2];
                c[l+8>>2]=c[q+8>>2];
                c[q>>2]=0;
                c[q+4>>2]=0;
                c[q+8>>2]=0;
                Ke(q);
                b=e
            }
            a[g>>0]=Qb[c[(c[e>>2]|0)+12>>2]&63](e)|0;
            a[h>>0]=Qb[c[(c[e>>2]|0)+16>>2]&63](e)|0;
            Nb[c[(c[b>>2]|0)+20>>2]&63](x,e);
            if(!(a[j>>0]&1))
            {
                a[j+1>>0]=0;
                a[j>>0]=0
            }
            else
            {
                a[c[j+8>>2]>>0]=0;
                c[j+4>>2]=0
            }
            Pe(j,0);
            c[j>>2]=c[x>>2];
            c[j+4>>2]=c[x+4>>2];
            c[j+8>>2]=c[x+8>>2];
            c[x>>2]=0;
            c[x+4>>2]=0;
            c[x+8>>2]=0;
            Ke(x);
            Nb[c[(c[b>>2]|0)+24>>2]&63](y,e);
            if(!(a[k>>0]&1))
            {
                a[k+1>>0]=0;
                a[k>>0]=0
            }
            else
            {
                a[c[k+8>>2]>>0]=0;
                c[k+4>>2]=0
            }
            Pe(k,0);
            c[k>>2]=c[y>>2];
            c[k+4>>2]=c[y+4>>2];
            c[k+8>>2]=c[y+8>>2];
            c[y>>2]=0;
            c[y+4>>2]=0;
            c[y+8>>2]=0;
            Ke(y);
            b=Qb[c[(c[e>>2]|0)+36>>2]&63](e)|0
        }
        else
        {
            e=tk(e,6104)|0;
            b=c[e>>2]|0;
            if(d)
            {
                Nb[c[b+44>>2]&63](r,e);
                d=c[r>>2]|0;
                a[f>>0]=d;
                a[f+1>>0]=d>>8;
                a[f+2>>0]=d>>16;
                a[f+3>>0]=d>>24;
                Nb[c[(c[e>>2]|0)+32>>2]&63](s,e);
                if(!(a[l>>0]&1))
                {
                    a[l+1>>0]=0;
                    a[l>>0]=0
                }
                else
                {
                    a[c[l+8>>2]>>0]=0;
                    c[l+4>>2]=0
                }
                Pe(l,0);
                c[l>>2]=c[s>>2];
                c[l+4>>2]=c[s+4>>2];
                c[l+8>>2]=c[s+8>>2];
                c[s>>2]=0;
                c[s+4>>2]=0;
                c[s+8>>2]=0;
                Ke(s);
                b=e
            }
            else
            {
                Nb[c[b+40>>2]&63](t,e);
                d=c[t>>2]|0;
                a[f>>0]=d;
                a[f+1>>0]=d>>8;
                a[f+2>>0]=d>>16;
                a[f+3>>0]=d>>24;
                Nb[c[(c[e>>2]|0)+28>>2]&63](u,e);
                if(!(a[l>>0]&1))
                {
                    a[l+1>>0]=0;
                    a[l>>0]=0
                }
                else
                {
                    a[c[l+8>>2]>>0]=0;
                    c[l+4>>2]=0
                }
                Pe(l,0);
                c[l>>2]=c[u>>2];
                c[l+4>>2]=c[u+4>>2];
                c[l+8>>2]=c[u+8>>2];
                c[u>>2]=0;
                c[u+4>>2]=0;
                c[u+8>>2]=0;
                Ke(u);
                b=e
            }
            a[g>>0]=Qb[c[(c[e>>2]|0)+12>>2]&63](e)|0;
            a[h>>0]=Qb[c[(c[e>>2]|0)+16>>2]&63](e)|0;
            Nb[c[(c[b>>2]|0)+20>>2]&63](v,e);
            if(!(a[j>>0]&1))
            {
                a[j+1>>0]=0;
                a[j>>0]=0
            }
            else
            {
                a[c[j+8>>2]>>0]=0;
                c[j+4>>2]=0
            }
            Pe(j,0);
            c[j>>2]=c[v>>2];
            c[j+4>>2]=c[v+4>>2];
            c[j+8>>2]=c[v+8>>2];
            c[v>>2]=0;
            c[v+4>>2]=0;
            c[v+8>>2]=0;
            Ke(v);
            Nb[c[(c[b>>2]|0)+24>>2]&63](w,e);
            if(!(a[k>>0]&1))
            {
                a[k+1>>0]=0;
                a[k>>0]=0
            }
            else
            {
                a[c[k+8>>2]>>0]=0;
                c[k+4>>2]=0
            }
            Pe(k,0);
            c[k>>2]=c[w>>2];
            c[k+4>>2]=c[w+4>>2];
            c[k+8>>2]=c[w+8>>2];
            c[w>>2]=0;
            c[w+4>>2]=0;
            c[w+8>>2]=0;
            Ke(w);
            b=Qb[c[(c[e>>2]|0)+36>>2]&63](e)|0
        }
        c[m>>2]=b;
        i=z;
        return
    }
    function Rj(d,e,f,g,h,i,j,k,l,m,n,o,p,q,r)
    {
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        n=n|0;
        o=o|0;
        p=p|0;
        q=q|0;
        r=r|0;
        var s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0;
        c[f>>2]=d;
        M=q+4|0;
        N=q+8|0;
        O=q+1|0;
        G=p+4|0;
        H=(g&512|0)==0;
        I=p+8|0;
        J=p+1|0;
        K=j+8|0;
        L=(r|0)>0;
        z=o+4|0;
        A=o+8|0;
        B=o+1|0;
        C=r+1|0;
        E=-2-r-((r|0)<0?~r:-1)|0;
        F=(r|0)>0;
        y=0;
        do
        {
            switch(a[l+y>>0]|0)
            {
                case 4:
                    {
                        u=c[f>>2]|0;
                        h=k?h+1|0:h;
                        w=h;
                        v=c[K>>2]|0;
                        a:do if(h>>>0<i>>>0)
                        {
                            s=h;
                            do
                            {
                                t=a[s>>0]|0;
                                if(t<<24>>24<=-1)break a;
                                if(!(b[v+(t<<24>>24<<1)>>1]&2048))break a;
                                s=s+1|0
                            }
                            while(s>>>0<i>>>0)
                        }
                        else s=h;
                        while(0);
                        t=s;
                        if(L)
                        {
                            x=-2-t-~(t>>>0>w>>>0?w:t)|0;
                            x=E>>>0>x>>>0?E:x;
                            if(s>>>0>h>>>0&F)
                            {
                                t=s;
                                w=r;
                                while(1)
                                {
                                    t=t+-1|0;
                                    P=a[t>>0]|0;
                                    v=c[f>>2]|0;
                                    c[f>>2]=v+1;
                                    a[v>>0]=P;
                                    v=(w|0)>1;
                                    if(!(t>>>0>h>>>0&v))break;
                                    else w=w+-1|0
                                }
                            }
                            else v=F;
                            w=C+x|0;
                            t=s+(x+1)|0;
                            if(v)v=Wb[c[(c[j>>2]|0)+28>>2]&15](j,48)|0;
                            else v=0;
                            s=c[f>>2]|0;
                            c[f>>2]=s+1;
                            if((w|0)>0)while(1)
                                {
                                a[s>>0]=v;
                                s=c[f>>2]|0;
                                c[f>>2]=s+1;
                                if((w|0)>1)w=w+-1|0;
                                else break
                            }
                            a[s>>0]=m
                        }
                        else t=s;
                        if((t|0)!=(h|0))
                        {
                            P=a[o>>0]|0;
                            s=(P&1)==0;
                            if(!((s?(P&255)>>>1:c[z>>2]|0)|0))s=-1;
                            else s=a[(s?B:c[A>>2]|0)>>0]|0;
                            if((t|0)!=(h|0))
                            {
                                v=0;
                                w=0;
                                while(1)
                                {
                                    if((w|0)==(s|0))
                                    {
                                        P=c[f>>2]|0;
                                        c[f>>2]=P+1;
                                        a[P>>0]=n;
                                        v=v+1|0;
                                        P=a[o>>0]|0;
                                        s=(P&1)==0;
                                        if(v>>>0<(s?(P&255)>>>1:c[z>>2]|0)>>>0)
                                        {
                                            s=a[(s?B:c[A>>2]|0)+v>>0]|0;
                                            s=s<<24>>24==127?-1:s<<24>>24;
                                            w=0
                                        }
                                        else
                                        {
                                            s=w;
                                            w=0
                                        }
                                    }
                                    t=t+-1|0;
                                    x=a[t>>0]|0;
                                    P=c[f>>2]|0;
                                    c[f>>2]=P+1;
                                    a[P>>0]=x;
                                    if((t|0)==(h|0))break;
                                    else w=w+1|0
                                }
                            }
                        }
                        else
                        {
                            x=Wb[c[(c[j>>2]|0)+28>>2]&15](j,48)|0;
                            P=c[f>>2]|0;
                            c[f>>2]=P+1;
                            a[P>>0]=x
                        }
                        s=c[f>>2]|0;
                        if((u|0)!=(s|0)?(D=s+-1|0,u>>>0<D>>>0):0)
                        {
                            s=D;
                            do
                            {
                                P=a[u>>0]|0;
                                a[u>>0]=a[s>>0]|0;
                                a[s>>0]=P;
                                u=u+1|0;
                                s=s+-1|0
                            }
                            while(u>>>0<s>>>0)
                        }
                        break
                    }
                case 3:
                    {
                        P=a[q>>0]|0;
                        u=(P&1)==0;
                        if((u?(P&255)>>>1:c[M>>2]|0)|0)
                        {
                            x=a[(u?O:c[N>>2]|0)>>0]|0;
                            P=c[f>>2]|0;
                            c[f>>2]=P+1;
                            a[P>>0]=x
                        }
                        break
                    }
                case 0:
                    {
                        c[e>>2]=c[f>>2];
                        break
                    }
                case 1:
                    {
                        c[e>>2]=c[f>>2];
                        x=Wb[c[(c[j>>2]|0)+28>>2]&15](j,32)|0;
                        P=c[f>>2]|0;
                        c[f>>2]=P+1;
                        a[P>>0]=x;
                        break
                    }
                case 2:
                    {
                        t=a[p>>0]|0;
                        u=(t&1)==0;
                        t=u?(t&255)>>>1:c[G>>2]|0;
                        if(!(H|(t|0)==0))
                        {
                            s=u?J:c[I>>2]|0;
                            w=s+t|0;
                            u=c[f>>2]|0;
                            if(t)do
                                {
                                a[u>>0]=a[s>>0]|0;
                                s=s+1|0;
                                u=u+1|0
                            }
                            while((s|0)!=(w|0));
                            c[f>>2]=u
                        }
                        break
                    }
                default:
                    {
                    }
            }
            y=y+1|0
        }
        while((y|0)!=4);
        t=a[q>>0]|0;
        h=(t&1)==0;
        t=h?(t&255)>>>1:c[M>>2]|0;
        if(t>>>0>1)
        {
            s=h?O:c[N>>2]|0;
            u=s+t|0;
            h=c[f>>2]|0;
            if((t|0)!=1)
            {
                s=s+1|0;
                do
                {
                    a[h>>0]=a[s>>0]|0;
                    h=h+1|0;
                    s=s+1|0
                }
                while((s|0)!=(u|0))
            }
            c[f>>2]=h
        }
        h=g&176;
        if((h|0)==32)c[e>>2]=c[f>>2];
        else if((h|0)!=16)c[e>>2]=d;
        return
    }
    function Sj(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0;
        D=i;
        i=i+176|0;
        p=D+56|0;
        x=D+52|0;
        v=D+60|0;
        s=D+164|0;
        w=D+165|0;
        A=D+40|0;
        C=D+28|0;
        B=D+12|0;
        l=D+8|0;
        o=D+64|0;
        u=D+4|0;
        t=D;
        q=D+24|0;
        b=ff(f)|0;
        c[x>>2]=b;
        r=tk(x,6584)|0;
        n=a[h>>0]|0;
        j=(n&1)==0;
        k=h+4|0;
        if(!((j?(n&255)>>>1:c[k>>2]|0)|0))n=0;
        else
        {
            n=a[(j?h+1|0:c[h+8>>2]|0)>>0]|0;
            n=n<<24>>24==(Wb[c[(c[r>>2]|0)+28>>2]&15](r,45)|0)<<24>>24
        }
        c[A>>2]=0;
        c[A+4>>2]=0;
        c[A+8>>2]=0;
        c[C>>2]=0;
        c[C+4>>2]=0;
        c[C+8>>2]=0;
        c[B>>2]=0;
        c[B+4>>2]=0;
        c[B+8>>2]=0;
        Qj(e,n,x,v,s,w,A,C,B,l);
        m=a[h>>0]|0;
        k=c[k>>2]|0;
        j=(m&1)==0?(m&255)>>>1:k;
        e=c[l>>2]|0;
        if((j|0)>(e|0))
        {
            E=a[B>>0]|0;
            l=a[C>>0]|0;
            j=(j-e<<1|1)+e+((E&1)==0?(E&255)>>>1:c[B+4>>2]|0)+((l&1)==0?(l&255)>>>1:c[C+4>>2]|0)|0
        }
        else
        {
            l=a[B>>0]|0;
            j=a[C>>0]|0;
            j=e+2+((l&1)==0?(l&255)>>>1:c[B+4>>2]|0)+((j&1)==0?(j&255)>>>1:c[C+4>>2]|0)|0
        }
        if(j>>>0>100)
        {
            j=$d(j)|0;
            if(!j)sd();
            else
            {
                z=j;
                y=j
            }
        }
        else
        {
            z=0;
            y=o
        }
        j=(m&1)==0;
        h=j?h+1|0:c[h+8>>2]|0;
        Rj(y,u,t,c[f+4>>2]|0,h,h+(j?(m&255)>>>1:k)|0,r,n,v,a[s>>0]|0,a[w>>0]|0,A,C,B,e);
        c[q>>2]=c[d>>2];
        d=c[u>>2]|0;
        j=c[t>>2]|0;
        c[p>>2]=c[q>>2];
        j=kc(p,y,d,j,f,g)|0;
        if(z)
        {
            ae(z);
            b=c[x>>2]|0
        }
        Ke(B);
        Ke(C);
        Ke(A);
        Sn(b)|0;
        i=D;
        return j|0
    }
    function Tj(a)
    {
        a=a|0;
        return
    }
    function Uj(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Vj(b,d,e,f,g,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        j=+j;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0;
        F=i;
        i=i+992|0;
        q=F+8|0;
        l=F;
        b=F+888|0;
        m=F+880|0;
        k=F+480|0;
        y=F+472|0;
        v=F+988|0;
        s=F+468|0;
        w=F+884|0;
        C=F+44|0;
        E=F+16|0;
        D=F+28|0;
        o=F+464|0;
        p=F+56|0;
        u=F+456|0;
        t=F+460|0;
        r=F+40|0;
        c[m>>2]=b;
        h[q>>3]=j;
        b=Rd(b,100,9920,q)|0;
        if(b>>>0>99)
        {
            b=Jg()|0;
            h[l>>3]=j;
            b=Bm(m,b,9920,l)|0;
            k=c[m>>2]|0;
            if(!k)sd();
            l=$d(b<<2)|0;
            if(!l)sd();
            else
            {
                G=l;
                H=k;
                x=l;
                A=b
            }
        }
        else
        {
            G=0;
            H=0;
            x=k;
            A=b
        }
        b=ff(f)|0;
        c[y>>2]=b;
        n=tk(y,6576)|0;
        l=c[m>>2]|0;
        Ub[c[(c[n>>2]|0)+48>>2]&7](n,l,l+A|0,x)|0;
        if(!A)m=0;
        else m=(a[c[m>>2]>>0]|0)==45;
        c[C>>2]=0;
        c[C+4>>2]=0;
        c[C+8>>2]=0;
        c[E>>2]=0;
        c[E+4>>2]=0;
        c[E+8>>2]=0;
        c[D>>2]=0;
        c[D+4>>2]=0;
        c[D+8>>2]=0;
        Wj(e,m,y,v,s,w,C,E,D,o);
        l=c[o>>2]|0;
        if((A|0)>(l|0))
        {
            o=a[D>>0]|0;
            k=a[E>>0]|0;
            k=(A-l<<1|1)+l+((o&1)==0?(o&255)>>>1:c[D+4>>2]|0)+((k&1)==0?(k&255)>>>1:c[E+4>>2]|0)|0
        }
        else
        {
            o=a[D>>0]|0;
            k=a[E>>0]|0;
            k=l+2+((o&1)==0?(o&255)>>>1:c[D+4>>2]|0)+((k&1)==0?(k&255)>>>1:c[E+4>>2]|0)|0
        }
        if(k>>>0>100)
        {
            k=$d(k<<2)|0;
            if(!k)sd();
            else
            {
                B=k;
                z=k
            }
        }
        else
        {
            B=0;
            z=p
        }
        Xj(z,u,t,c[f+4>>2]|0,x,x+(A<<2)|0,n,m,v,c[s>>2]|0,c[w>>2]|0,C,E,D,l);
        c[r>>2]=c[d>>2];
        d=c[u>>2]|0;
        k=c[t>>2]|0;
        c[q>>2]=c[r>>2];
        k=Cm(q,z,d,k,f,g)|0;
        if(B)
        {
            ae(B);
            b=c[y>>2]|0
        }
        Ve(D);
        Ve(E);
        Ke(C);
        Sn(b)|0;
        if(G)ae(G);
        if(H)ae(H);
        i=F;
        return k|0
    }
    function Wj(b,d,e,f,g,h,j,k,l,m)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0;
        z=i;
        i=i+112|0;
        n=z+108|0;
        o=z+96|0;
        r=z+92|0;
        s=z+80|0;
        t=z+68|0;
        u=z+56|0;
        v=z+52|0;
        w=z+28|0;
        x=z+24|0;
        y=z+12|0;
        p=z;
        q=z+40|0;
        if(b)
        {
            b=tk(e,6296)|0;
            e=c[b>>2]|0;
            if(d)
            {
                Nb[c[e+44>>2]&63](n,b);
                d=c[n>>2]|0;
                a[f>>0]=d;
                a[f+1>>0]=d>>8;
                a[f+2>>0]=d>>16;
                a[f+3>>0]=d>>24;
                Nb[c[(c[b>>2]|0)+32>>2]&63](o,b);
                if(!(a[l>>0]&1))a[l>>0]=0;
                else c[c[l+8>>2]>>2]=0;
                c[l+4>>2]=0;
                Ye(l,0);
                c[l>>2]=c[o>>2];
                c[l+4>>2]=c[o+4>>2];
                c[l+8>>2]=c[o+8>>2];
                c[o>>2]=0;
                c[o+4>>2]=0;
                c[o+8>>2]=0;
                Ve(o)
            }
            else
            {
                Nb[c[e+40>>2]&63](r,b);
                d=c[r>>2]|0;
                a[f>>0]=d;
                a[f+1>>0]=d>>8;
                a[f+2>>0]=d>>16;
                a[f+3>>0]=d>>24;
                Nb[c[(c[b>>2]|0)+28>>2]&63](s,b);
                if(!(a[l>>0]&1))a[l>>0]=0;
                else c[c[l+8>>2]>>2]=0;
                c[l+4>>2]=0;
                Ye(l,0);
                c[l>>2]=c[s>>2];
                c[l+4>>2]=c[s+4>>2];
                c[l+8>>2]=c[s+8>>2];
                c[s>>2]=0;
                c[s+4>>2]=0;
                c[s+8>>2]=0;
                Ve(s)
            }
            c[g>>2]=Qb[c[(c[b>>2]|0)+12>>2]&63](b)|0;
            c[h>>2]=Qb[c[(c[b>>2]|0)+16>>2]&63](b)|0;
            Nb[c[(c[b>>2]|0)+20>>2]&63](t,b);
            if(!(a[j>>0]&1))
            {
                a[j+1>>0]=0;
                a[j>>0]=0
            }
            else
            {
                a[c[j+8>>2]>>0]=0;
                c[j+4>>2]=0
            }
            Pe(j,0);
            c[j>>2]=c[t>>2];
            c[j+4>>2]=c[t+4>>2];
            c[j+8>>2]=c[t+8>>2];
            c[t>>2]=0;
            c[t+4>>2]=0;
            c[t+8>>2]=0;
            Ke(t);
            Nb[c[(c[b>>2]|0)+24>>2]&63](u,b);
            if(!(a[k>>0]&1))a[k>>0]=0;
            else c[c[k+8>>2]>>2]=0;
            c[k+4>>2]=0;
            Ye(k,0);
            c[k>>2]=c[u>>2];
            c[k+4>>2]=c[u+4>>2];
            c[k+8>>2]=c[u+8>>2];
            c[u>>2]=0;
            c[u+4>>2]=0;
            c[u+8>>2]=0;
            Ve(u);
            b=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0
        }
        else
        {
            b=tk(e,6232)|0;
            e=c[b>>2]|0;
            if(d)
            {
                Nb[c[e+44>>2]&63](v,b);
                d=c[v>>2]|0;
                a[f>>0]=d;
                a[f+1>>0]=d>>8;
                a[f+2>>0]=d>>16;
                a[f+3>>0]=d>>24;
                Nb[c[(c[b>>2]|0)+32>>2]&63](w,b);
                if(!(a[l>>0]&1))a[l>>0]=0;
                else c[c[l+8>>2]>>2]=0;
                c[l+4>>2]=0;
                Ye(l,0);
                c[l>>2]=c[w>>2];
                c[l+4>>2]=c[w+4>>2];
                c[l+8>>2]=c[w+8>>2];
                c[w>>2]=0;
                c[w+4>>2]=0;
                c[w+8>>2]=0;
                Ve(w)
            }
            else
            {
                Nb[c[e+40>>2]&63](x,b);
                d=c[x>>2]|0;
                a[f>>0]=d;
                a[f+1>>0]=d>>8;
                a[f+2>>0]=d>>16;
                a[f+3>>0]=d>>24;
                Nb[c[(c[b>>2]|0)+28>>2]&63](y,b);
                if(!(a[l>>0]&1))a[l>>0]=0;
                else c[c[l+8>>2]>>2]=0;
                c[l+4>>2]=0;
                Ye(l,0);
                c[l>>2]=c[y>>2];
                c[l+4>>2]=c[y+4>>2];
                c[l+8>>2]=c[y+8>>2];
                c[y>>2]=0;
                c[y+4>>2]=0;
                c[y+8>>2]=0;
                Ve(y)
            }
            c[g>>2]=Qb[c[(c[b>>2]|0)+12>>2]&63](b)|0;
            c[h>>2]=Qb[c[(c[b>>2]|0)+16>>2]&63](b)|0;
            Nb[c[(c[b>>2]|0)+20>>2]&63](p,b);
            if(!(a[j>>0]&1))
            {
                a[j+1>>0]=0;
                a[j>>0]=0
            }
            else
            {
                a[c[j+8>>2]>>0]=0;
                c[j+4>>2]=0
            }
            Pe(j,0);
            c[j>>2]=c[p>>2];
            c[j+4>>2]=c[p+4>>2];
            c[j+8>>2]=c[p+8>>2];
            c[p>>2]=0;
            c[p+4>>2]=0;
            c[p+8>>2]=0;
            Ke(p);
            Nb[c[(c[b>>2]|0)+24>>2]&63](q,b);
            if(!(a[k>>0]&1))a[k>>0]=0;
            else c[c[k+8>>2]>>2]=0;
            c[k+4>>2]=0;
            Ye(k,0);
            c[k>>2]=c[q>>2];
            c[k+4>>2]=c[q+4>>2];
            c[k+8>>2]=c[q+8>>2];
            c[q>>2]=0;
            c[q+4>>2]=0;
            c[q+8>>2]=0;
            Ve(q);
            b=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0
        }
        c[m>>2]=b;
        i=z;
        return
    }
    function Xj(b,d,e,f,g,h,i,j,k,l,m,n,o,p,q)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        k=k|0;
        l=l|0;
        m=m|0;
        n=n|0;
        o=o|0;
        p=p|0;
        q=q|0;
        var r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0;
        c[e>>2]=b;
        J=p+4|0;
        K=p+8|0;
        C=o+4|0;
        D=(f&512|0)==0;
        E=o+8|0;
        F=(q|0)>0;
        G=n+4|0;
        H=n+8|0;
        I=n+1|0;
        A=(q|0)>0;
        z=0;
        do
        {
            switch(a[k+z>>0]|0)
            {
                case 1:
                    {
                        c[d>>2]=c[e>>2];
                        u=Wb[c[(c[i>>2]|0)+44>>2]&15](i,32)|0;
                        w=c[e>>2]|0;
                        c[e>>2]=w+4;
                        c[w>>2]=u;
                        break
                    }
                case 2:
                    {
                        v=a[o>>0]|0;
                        t=(v&1)==0;
                        v=t?(v&255)>>>1:c[C>>2]|0;
                        if(!(D|(v|0)==0))
                        {
                            t=t?C:c[E>>2]|0;
                            s=t+(v<<2)|0;
                            u=c[e>>2]|0;
                            if(v)
                            {
                                r=u;
                                while(1)
                                {
                                    c[r>>2]=c[t>>2];
                                    t=t+4|0;
                                    if((t|0)==(s|0))break;
                                    else r=r+4|0
                                }
                            }
                            c[e>>2]=u+(v<<2)
                        }
                        break
                    }
                case 4:
                    {
                        t=c[e>>2]|0;
                        g=j?g+4|0:g;
                        a:do if(g>>>0<h>>>0)
                        {
                            r=g;
                            do
                            {
                                if(!(Jb[c[(c[i>>2]|0)+12>>2]&31](i,2048,c[r>>2]|0)|0))break a;
                                r=r+4|0
                            }
                            while(r>>>0<h>>>0)
                        }
                        else r=g;
                        while(0);
                        if(F)
                        {
                            if(r>>>0>g>>>0&A)
                            {
                                u=c[e>>2]|0;
                                v=q;
                                while(1)
                                {
                                    r=r+-4|0;
                                    s=u+4|0;
                                    c[u>>2]=c[r>>2];
                                    x=v+-1|0;
                                    u=(v|0)>1;
                                    if(r>>>0>g>>>0&u)
                                    {
                                        u=s;
                                        v=x
                                    }
                                    else break
                                }
                                c[e>>2]=s;
                                s=x
                            }
                            else
                            {
                                u=A;
                                s=q
                            }
                            if(u)w=Wb[c[(c[i>>2]|0)+44>>2]&15](i,48)|0;
                            else w=0;
                            x=c[e>>2]|0;
                            v=s+((s|0)<0?~s:-1)|0;
                            if((s|0)>0)
                            {
                                u=x;
                                while(1)
                                {
                                    c[u>>2]=w;
                                    if((s|0)>1)
                                    {
                                        u=u+4|0;
                                        s=s+-1|0
                                    }
                                    else break
                                }
                            }
                            c[e>>2]=x+(v+2<<2);
                            c[x+(v+1<<2)>>2]=l
                        }
                        if((r|0)==(g|0))
                        {
                            u=Wb[c[(c[i>>2]|0)+44>>2]&15](i,48)|0;
                            w=c[e>>2]|0;
                            r=w+4|0;
                            c[e>>2]=r;
                            c[w>>2]=u
                        }
                        else
                        {
                            w=a[n>>0]|0;
                            s=(w&1)==0;
                            y=c[G>>2]|0;
                            if(!((s?(w&255)>>>1:y)|0))s=-1;
                            else s=a[(s?I:c[H>>2]|0)>>0]|0;
                            if((r|0)!=(g|0))
                            {
                                u=0;
                                w=0;
                                while(1)
                                {
                                    v=c[e>>2]|0;
                                    if((w|0)==(s|0))
                                    {
                                        x=v+4|0;
                                        c[e>>2]=x;
                                        c[v>>2]=m;
                                        u=u+1|0;
                                        v=a[n>>0]|0;
                                        s=(v&1)==0;
                                        if(u>>>0<(s?(v&255)>>>1:y)>>>0)
                                        {
                                            s=a[(s?I:c[H>>2]|0)+u>>0]|0;
                                            v=x;
                                            s=s<<24>>24==127?-1:s<<24>>24;
                                            x=0
                                        }
                                        else
                                        {
                                            v=x;
                                            s=w;
                                            x=0
                                        }
                                    }
                                    else x=w;
                                    r=r+-4|0;
                                    w=c[r>>2]|0;
                                    c[e>>2]=v+4;
                                    c[v>>2]=w;
                                    if((r|0)==(g|0))break;
                                    else w=x+1|0
                                }
                            }
                            r=c[e>>2]|0
                        }
                        if((t|0)!=(r|0)?(B=r+-4|0,t>>>0<B>>>0):0)
                        {
                            r=B;
                            do
                            {
                                w=c[t>>2]|0;
                                c[t>>2]=c[r>>2];
                                c[r>>2]=w;
                                t=t+4|0;
                                r=r+-4|0
                            }
                            while(t>>>0<r>>>0)
                        }
                        break
                    }
                case 0:
                    {
                        c[d>>2]=c[e>>2];
                        break
                    }
                case 3:
                    {
                        w=a[p>>0]|0;
                        t=(w&1)==0;
                        if((t?(w&255)>>>1:c[J>>2]|0)|0)
                        {
                            u=c[(t?J:c[K>>2]|0)>>2]|0;
                            w=c[e>>2]|0;
                            c[e>>2]=w+4;
                            c[w>>2]=u
                        }
                        break
                    }
                default:
                    {
                    }
            }
            z=z+1|0
        }
        while((z|0)!=4);
        r=a[p>>0]|0;
        g=(r&1)==0;
        r=g?(r&255)>>>1:c[J>>2]|0;
        if(r>>>0>1)
        {
            s=g?J:c[K>>2]|0;
            g=s+4|0;
            s=s+(r<<2)|0;
            t=c[e>>2]|0;
            u=s-g|0;
            if((r|0)!=1)
            {
                r=t;
                while(1)
                {
                    c[r>>2]=c[g>>2];
                    g=g+4|0;
                    if((g|0)==(s|0))break;
                    else r=r+4|0
                }
            }
            c[e>>2]=t+(u>>>2<<2)
        }
        g=f&176;
        if((g|0)==32)c[d>>2]=c[e>>2];
        else if((g|0)!=16)c[d>>2]=b;
        return
    }
    function Yj(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0;
        E=i;
        i=i+480|0;
        p=E+464|0;
        y=E+460|0;
        w=E+468|0;
        t=E+456|0;
        x=E+452|0;
        B=E+440|0;
        D=E+428|0;
        C=E+412|0;
        k=E+408|0;
        o=E+8|0;
        v=E+4|0;
        u=E;
        q=E+424|0;
        b=ff(f)|0;
        c[y>>2]=b;
        r=tk(y,6576)|0;
        n=a[h>>0]|0;
        j=(n&1)==0;
        s=h+4|0;
        if(!((j?(n&255)>>>1:c[s>>2]|0)|0))n=0;
        else
        {
            n=c[(j?s:c[h+8>>2]|0)>>2]|0;
            n=(n|0)==(Wb[c[(c[r>>2]|0)+44>>2]&15](r,45)|0)
        }
        c[B>>2]=0;
        c[B+4>>2]=0;
        c[B+8>>2]=0;
        c[D>>2]=0;
        c[D+4>>2]=0;
        c[D+8>>2]=0;
        c[C>>2]=0;
        c[C+4>>2]=0;
        c[C+8>>2]=0;
        Wj(e,n,y,w,t,x,B,D,C,k);
        l=a[h>>0]|0;
        m=c[s>>2]|0;
        j=(l&1)==0?(l&255)>>>1:m;
        e=c[k>>2]|0;
        if((j|0)>(e|0))
        {
            F=a[C>>0]|0;
            k=a[D>>0]|0;
            j=(j-e<<1|1)+e+((F&1)==0?(F&255)>>>1:c[C+4>>2]|0)+((k&1)==0?(k&255)>>>1:c[D+4>>2]|0)|0
        }
        else
        {
            F=a[C>>0]|0;
            j=a[D>>0]|0;
            j=e+2+((F&1)==0?(F&255)>>>1:c[C+4>>2]|0)+((j&1)==0?(j&255)>>>1:c[D+4>>2]|0)|0
        }
        if(j>>>0>100)
        {
            j=$d(j<<2)|0;
            if(!j)sd();
            else
            {
                A=j;
                z=j
            }
        }
        else
        {
            A=0;
            z=o
        }
        F=(l&1)==0;
        j=F?s:c[h+8>>2]|0;
        Xj(z,v,u,c[f+4>>2]|0,j,j+((F?(l&255)>>>1:m)<<2)|0,r,n,w,c[t>>2]|0,c[x>>2]|0,B,D,C,e);
        c[q>>2]=c[d>>2];
        F=c[v>>2]|0;
        j=c[u>>2]|0;
        c[p>>2]=c[q>>2];
        j=Cm(p,z,F,j,f,g)|0;
        if(A)
        {
            ae(A);
            b=c[y>>2]|0
        }
        Ve(C);
        Ve(D);
        Ke(B);
        Sn(b)|0;
        i=E;
        return j|0
    }
    function Zj(a)
    {
        a=a|0;
        return
    }
    function _j(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function $j(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        b=Fb(((a[d>>0]&1)==0?d+1|0:c[d+8>>2]|0)|0,1)|0;
        return b>>>((b|0)!=(-1|0)&1)|0
    }
    function ak(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0;
        k=i;
        i=i+16|0;
        j=k;
        c[j>>2]=0;
        c[j+4>>2]=0;
        c[j+8>>2]=0;
        l=a[h>>0]|0;
        m=(l&1)==0;
        d=m?h+1|0:c[h+8>>2]|0;
        l=m?(l&255)>>>1:c[h+4>>2]|0;
        h=d+l|0;
        if((l|0)>0)do
            {
            Qe(j,a[d>>0]|0);
            d=d+1|0
        }
        while(d>>>0<h>>>0);
        d=Wa(((e|0)==-1?-1:e<<1)|0,f|0,g|0,((a[j>>0]&1)==0?j+1|0:c[j+8>>2]|0)|0)|0;
        c[b>>2]=0;
        c[b+4>>2]=0;
        c[b+8>>2]=0;
        m=Yn(d|0)|0;
        h=d+m|0;
        if((m|0)>0)do
            {
            Qe(b,a[d>>0]|0);
            d=d+1|0
        }
        while(d>>>0<h>>>0);
        Ke(j);
        i=k;
        return
    }
    function bk(a,b)
    {
        a=a|0;
        b=b|0;
        yb(((b|0)==-1?-1:b<<1)|0)|0;
        return
    }
    function ck(a)
    {
        a=a|0;
        return
    }
    function dk(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function ek(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        b=Fb(((a[d>>0]&1)==0?d+1|0:c[d+8>>2]|0)|0,1)|0;
        return b>>>((b|0)!=(-1|0)&1)|0
    }
    function fk(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0;
        s=i;
        i=i+176|0;
        p=s+168|0;
        o=s;
        n=s+128|0;
        r=s+132|0;
        q=s+136|0;
        l=s+152|0;
        m=s+160|0;
        c[q>>2]=0;
        c[q+4>>2]=0;
        c[q+8>>2]=0;
        c[l+4>>2]=0;
        c[l>>2]=7384;
        j=a[h>>0]|0;
        t=(j&1)==0;
        k=h+4|0;
        d=t?k:c[h+8>>2]|0;
        k=t?(j&255)>>>1:c[k>>2]|0;
        h=d+(k<<2)|0;
        j=o+32|0;
        if((k|0)>0)do
            {
            c[r>>2]=d;
            k=Tb[c[(c[l>>2]|0)+12>>2]&15](l,p,d,h,r,o,j,n)|0;
            if(o>>>0<(c[n>>2]|0)>>>0)
            {
                d=o;
                do
                {
                    Qe(q,a[d>>0]|0);
                    d=d+1|0
                }
                while(d>>>0<(c[n>>2]|0)>>>0)
            }
            d=c[r>>2]|0
        }
        while((k|0)!=2&d>>>0<h>>>0);
        d=Wa(((e|0)==-1?-1:e<<1)|0,f|0,g|0,((a[q>>0]&1)==0?q+1|0:c[q+8>>2]|0)|0)|0;
        c[b>>2]=0;
        c[b+4>>2]=0;
        c[b+8>>2]=0;
        c[m+4>>2]=0;
        c[m>>2]=7488;
        t=Yn(d|0)|0;
        j=d+t|0;
        k=j;
        l=o+128|0;
        if((t|0)>0)do
            {
            c[r>>2]=d;
            h=Tb[c[(c[m>>2]|0)+16>>2]&15](m,p,d,(k-d|0)>32?d+32|0:j,r,o,l,n)|0;
            if(o>>>0<(c[n>>2]|0)>>>0)
            {
                d=o;
                do
                {
                    Ze(b,c[d>>2]|0);
                    d=d+4|0
                }
                while(d>>>0<(c[n>>2]|0)>>>0)
            }
            d=c[r>>2]|0
        }
        while((h|0)!=2&d>>>0<j>>>0);
        Ke(q);
        i=s;
        return
    }
    function gk(a,b)
    {
        a=a|0;
        b=b|0;
        yb(((b|0)==-1?-1:b<<1)|0)|0;
        return
    }
    function hk(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0;
        c[a+4>>2]=b+-1;
        c[a>>2]=6552;
        d=a+8|0;
        Km(d,28);
        Ie(a+144|0,9560,1);
        d=c[d>>2]|0;
        e=a+12|0;
        b=c[e>>2]|0;
        if((b|0)!=(d|0))
        {
            do b=b+-4|0;
            while((b|0)!=(d|0));
            c[e>>2]=b
        }
        c[2483]=0;
        c[2482]=5408;
        Lm(a,9928);
        c[2485]=0;
        c[2484]=5448;
        Mm(a,9936);
        Kk(9944,0,0,1);
        Nm(a,9944);
        c[2491]=0;
        c[2490]=6896;
        Om(a,9960);
        c[2493]=0;
        c[2492]=7024;
        Pm(a,9968);
        c[2495]=0;
        c[2494]=6672;
        c[2496]=Jg()|0;
        Qm(a,9976);
        c[2499]=0;
        c[2498]=7144;
        Rm(a,9992);
        c[2501]=0;
        c[2500]=7264;
        Sm(a,1e4);
        Bl(10008,1);
        Tm(a,10008);
        Cl(10032,1);
        Um(a,10032);
        c[2517]=0;
        c[2516]=5488;
        Vm(a,10064);
        c[2519]=0;
        c[2518]=5600;
        Wm(a,10072);
        c[2521]=0;
        c[2520]=5672;
        Xm(a,10080);
        c[2523]=0;
        c[2522]=5736;
        Ym(a,10088);
        c[2525]=0;
        c[2524]=6056;
        Zm(a,10096);
        c[2527]=0;
        c[2526]=6120;
        _m(a,10104);
        c[2529]=0;
        c[2528]=6184;
        $m(a,10112);
        c[2531]=0;
        c[2530]=6248;
        an(a,10120);
        c[2533]=0;
        c[2532]=6312;
        bn(a,10128);
        c[2535]=0;
        c[2534]=6352;
        cn(a,10136);
        c[2537]=0;
        c[2536]=6392;
        dn(a,10144);
        c[2539]=0;
        c[2538]=6432;
        en(a,10152);
        c[2541]=0;
        c[2540]=5800;
        c[2542]=5848;
        fn(a,10160);
        c[2545]=0;
        c[2544]=5896;
        c[2546]=5944;
        gn(a,10176);
        c[2549]=0;
        c[2548]=6832;
        c[2550]=Jg()|0;
        c[2548]=5992;
        hn(a,10192);
        c[2553]=0;
        c[2552]=6832;
        c[2554]=Jg()|0;
        c[2552]=6024;
        jn(a,10208);
        c[2557]=0;
        c[2556]=6472;
        kn(a,10224);
        c[2559]=0;
        c[2558]=6512;
        ln(a,10232);
        return
    }
    function ik()
    {
        if((a[10240]|0)==0?(Ea(10240)|0)!=0:0)
        {
            mk()|0;
            c[2564]=10248;
            Na(10240)
        }
        return c[2564]|0
    }
    function jk(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0;
        Rn(b);
        f=a+8|0;
        e=c[f>>2]|0;
        if((c[a+12>>2]|0)-e>>2>>>0<=d>>>0)
        {
            mn(f,d+1|0);
            e=c[f>>2]|0
        }
        a=c[e+(d<<2)>>2]|0;
        if(a)
        {
            Sn(a)|0;
            e=c[f>>2]|0
        }
        c[e+(d<<2)>>2]=b;
        return
    }
    function kk(a)
    {
        a=a|0;
        var b=0,d=0,e=0,f=0;
        c[a>>2]=6552;
        e=a+8|0;
        f=a+12|0;
        b=c[e>>2]|0;
        if((c[f>>2]|0)!=(b|0))
        {
            d=0;
            do
            {
                b=c[b+(d<<2)>>2]|0;
                if(b)Sn(b)|0;
                d=d+1|0;
                b=c[e>>2]|0
            }
            while(d>>>0<(c[f>>2]|0)-b>>2>>>0)
        }
        Ke(a+144|0);
        nn(e);
        return
    }
    function lk(a)
    {
        a=a|0;
        kk(a);
        Lc(a);
        return
    }
    function mk()
    {
        hk(10264,1);
        c[2562]=10264;
        return 10248
    }
    function nk()
    {
        var a=0;
        a=c[(ik()|0)>>2]|0;
        c[2606]=a;
        Rn(a);
        return 10424
    }
    function ok()
    {
        if((a[10432]|0)==0?(Ea(10432)|0)!=0:0)
        {
            nk()|0;
            c[2610]=10424;
            Na(10432)
        }
        return c[2610]|0
    }
    function pk(a)
    {
        a=a|0;
        var b=0;
        b=c[(ok()|0)>>2]|0;
        c[a>>2]=b;
        Rn(b);
        return
    }
    function qk(a,b)
    {
        a=a|0;
        b=b|0;
        b=c[b>>2]|0;
        c[a>>2]=b;
        Rn(b);
        return
    }
    function rk(a)
    {
        a=a|0;
        Sn(c[a>>2]|0)|0;
        return
    }
    function sk(a)
    {
        a=a|0;
        var b=0,d=0;
        d=i;
        i=i+16|0;
        b=d;
        if((c[a>>2]|0)!=-1)
        {
            c[b>>2]=a;
            c[b+4>>2]=97;
            c[b+8>>2]=0;
            Tn(a,b,98)
        }
        i=d;
        return (c[a+4>>2]|0)+-1|0
    }
    function tk(a,b)
    {
        a=a|0;
        b=b|0;
        a=c[a>>2]|0;
        b=sk(b)|0;
        return c[(c[a+8>>2]|0)+(b<<2)>>2]|0
    }
    function uk(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function vk(a)
    {
        a=a|0;
        if(a)Mb[c[(c[a>>2]|0)+4>>2]&127](a);
        return
    }
    function wk(a)
    {
        a=a|0;
        var b=0;
        b=c[1642]|0;
        c[1642]=b+1;
        c[a+4>>2]=b+1;
        return
    }
    function xk(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function yk(a,d,e)
    {
        a=a|0;
        d=d|0;
        e=e|0;
        if(e>>>0<128)e=(b[(c[(Da()|0)>>2]|0)+(e<<1)>>1]&d)<<16>>16!=0;
        else e=0;
        return e|0
    }
    function zk(a,d,f,g)
    {
        a=a|0;
        d=d|0;
        f=f|0;
        g=g|0;
        var h=0,i=0;
        i=(f-d|0)>>>2;
        if((d|0)!=(f|0))
        {
            h=d;
            while(1)
            {
                a=c[h>>2]|0;
                if(a>>>0<128)a=e[(c[(Da()|0)>>2]|0)+(a<<1)>>1]|0;
                else a=0;
                b[g>>1]=a;
                h=h+4|0;
                if((h|0)==(f|0))break;
                else g=g+2|0
            }
        }
        return d+(i<<2)|0
    }
    function Ak(a,d,e,f)
    {
        a=a|0;
        d=d|0;
        e=e|0;
        f=f|0;
        a:do if((e|0)==(f|0))e=f;
        else while(1)
            {
            a=c[e>>2]|0;
            if(a>>>0<128?(b[(c[(Da()|0)>>2]|0)+(a<<1)>>1]&d)<<16>>16!=0:0)break a;
            e=e+4|0;
            if((e|0)==(f|0))
            {
                e=f;
                break
            }
        }
        while(0);
        return e|0
    }
    function Bk(a,d,e,f)
    {
        a=a|0;
        d=d|0;
        e=e|0;
        f=f|0;
        a:do if((e|0)==(f|0))e=f;
        else while(1)
            {
            a=c[e>>2]|0;
            if(a>>>0>=128)break a;
            if(!((b[(c[(Da()|0)>>2]|0)+(a<<1)>>1]&d)<<16>>16))break a;
            e=e+4|0;
            if((e|0)==(f|0))
            {
                e=f;
                break
            }
        }
        while(0);
        return e|0
    }
    function Ck(a,b)
    {
        a=a|0;
        b=b|0;
        if(b>>>0<128)b=c[(c[(La()|0)>>2]|0)+(b<<2)>>2]|0;
        return b|0
    }
    function Dk(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0;
        f=(d-b|0)>>>2;
        if((b|0)!=(d|0))
        {
            e=b;
            do
            {
                a=c[e>>2]|0;
                if(a>>>0<128)a=c[(c[(La()|0)>>2]|0)+(a<<2)>>2]|0;
                c[e>>2]=a;
                e=e+4|0
            }
            while((e|0)!=(d|0))
        }
        return b+(f<<2)|0
    }
    function Ek(a,b)
    {
        a=a|0;
        b=b|0;
        if(b>>>0<128)b=c[(c[(fb()|0)>>2]|0)+(b<<2)>>2]|0;
        return b|0
    }
    function Fk(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0;
        f=(d-b|0)>>>2;
        if((b|0)!=(d|0))
        {
            e=b;
            do
            {
                a=c[e>>2]|0;
                if(a>>>0<128)a=c[(c[(fb()|0)>>2]|0)+(a<<2)>>2]|0;
                c[e>>2]=a;
                e=e+4|0
            }
            while((e|0)!=(d|0))
        }
        return b+(f<<2)|0
    }
    function Gk(a,b)
    {
        a=a|0;
        b=b|0;
        return b<<24>>24|0
    }
    function Hk(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        if((d|0)!=(e|0))while(1)
            {
            c[f>>2]=a[d>>0];
            d=d+1|0;
            if((d|0)==(e|0))break;
            else f=f+4|0
        }
        return e|0
    }
    function Ik(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        return (b>>>0<128?b&255:c)|0
    }
    function Jk(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,i=0;
        i=(e-d|0)>>>2;
        if((d|0)!=(e|0))
        {
            h=d;
            b=g;
            while(1)
            {
                g=c[h>>2]|0;
                a[b>>0]=g>>>0<128?g&255:f;
                h=h+4|0;
                if((h|0)==(e|0))break;
                else b=b+1|0
            }
        }
        return d+(i<<2)|0
    }
    function Kk(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        c[b+4>>2]=f+-1;
        c[b>>2]=6600;
        f=b+8|0;
        c[f>>2]=d;
        a[b+12>>0]=e&1;
        if(!d)c[f>>2]=c[(Da()|0)>>2];
        return
    }
    function Lk(b)
    {
        b=b|0;
        var d=0;
        c[b>>2]=6600;
        d=c[b+8>>2]|0;
        if((d|0)!=0?(a[b+12>>0]|0)!=0:0)Mc(d);
        return
    }
    function Mk(a)
    {
        a=a|0;
        Lk(a);
        Lc(a);
        return
    }
    function Nk(a,b)
    {
        a=a|0;
        b=b|0;
        if(b<<24>>24>-1)b=c[(c[(La()|0)>>2]|0)+((b&255)<<2)>>2]&255;
        return b|0
    }
    function Ok(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        if((d|0)!=(e|0))
        {
            b=d;
            do
            {
                d=a[b>>0]|0;
                if(d<<24>>24>-1)d=c[(c[(La()|0)>>2]|0)+(d<<24>>24<<2)>>2]&255;
                a[b>>0]=d;
                b=b+1|0
            }
            while((b|0)!=(e|0))
        }
        return e|0
    }
    function Pk(a,b)
    {
        a=a|0;
        b=b|0;
        if(b<<24>>24>-1)b=c[(c[(fb()|0)>>2]|0)+(b<<24>>24<<2)>>2]&255;
        return b|0
    }
    function Qk(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        if((d|0)!=(e|0))
        {
            b=d;
            do
            {
                d=a[b>>0]|0;
                if(d<<24>>24>-1)d=c[(c[(fb()|0)>>2]|0)+(d<<24>>24<<2)>>2]&255;
                a[b>>0]=d;
                b=b+1|0
            }
            while((b|0)!=(e|0))
        }
        return e|0
    }
    function Rk(a,b)
    {
        a=a|0;
        b=b|0;
        return b|0
    }
    function Sk(b,c,d,e)
    {
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        if((c|0)!=(d|0))while(1)
            {
            a[e>>0]=a[c>>0]|0;
            c=c+1|0;
            if((c|0)==(d|0))break;
            else e=e+1|0
        }
        return d|0
    }
    function Tk(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        return (b<<24>>24>-1?b:c)|0
    }
    function Uk(b,c,d,e,f)
    {
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        if((c|0)!=(d|0))while(1)
            {
            b=a[c>>0]|0;
            a[f>>0]=b<<24>>24>-1?b:e;
            c=c+1|0;
            if((c|0)==(d|0))break;
            else f=f+1|0
        }
        return d|0
    }
    function Vk(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Wk(a,b,d,e,f,g,h,i)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        c[f>>2]=d;
        c[i>>2]=g;
        return 3
    }
    function Xk(a,b,d,e,f,g,h,i)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        c[f>>2]=d;
        c[i>>2]=g;
        return 3
    }
    function Yk(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        c[f>>2]=d;
        return 3
    }
    function Zk(a)
    {
        a=a|0;
        return 1
    }
    function _k(a)
    {
        a=a|0;
        return 1
    }
    function $k(a,b,c,d,e)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        a=d-c|0;
        return (a>>>0<e>>>0?a:e)|0
    }
    function al(a)
    {
        a=a|0;
        return 1
    }
    function bl(a)
    {
        a=a|0;
        em(a);
        Lc(a);
        return
    }
    function cl(b,d,e,f,g,h,j,k)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0;
        s=i;
        i=i+16|0;
        q=s;
        o=s+8|0;
        a:do if((e|0)==(f|0))l=f;
        else
        {
            l=e;
            while(1)
            {
                if(!(c[l>>2]|0))break a;
                l=l+4|0;
                if((l|0)==(f|0))
                {
                    l=f;
                    break
                }
            }
        }
        while(0);
        c[k>>2]=h;
        c[g>>2]=e;
        n=j;
        p=b+8|0;
        b:do if((h|0)==(j|0)|(e|0)==(f|0))r=29;
        else
        {
            while(1)
            {
                t=d;
                m=c[t+4>>2]|0;
                b=q;
                c[b>>2]=c[t>>2];
                c[b+4>>2]=m;
                b=Va(c[p>>2]|0)|0;
                m=vc(h,g,l-e>>2,n-h|0,d)|0;
                if(b)Va(b|0)|0;
                if((m|0)==-1)break;
                else if(!m)
                {
                    e=1;
                    break b
                }
                h=(c[k>>2]|0)+m|0;
                c[k>>2]=h;
                if((h|0)==(j|0))
                {
                    r=15;
                    break
                }
                if((l|0)==(f|0))
                {
                    e=c[g>>2]|0;
                    l=f
                }
                else
                {
                    e=Va(c[p>>2]|0)|0;
                    h=Md(o,0,d)|0;
                    if(e)Va(e|0)|0;
                    if((h|0)==-1)
                    {
                        e=2;
                        break b
                    }
                    if(h>>>0>(n-(c[k>>2]|0)|0)>>>0)
                    {
                        e=1;
                        break b
                    }
                    if(h)
                    {
                        e=o;
                        while(1)
                        {
                            m=a[e>>0]|0;
                            t=c[k>>2]|0;
                            c[k>>2]=t+1;
                            a[t>>0]=m;
                            h=h+-1|0;
                            if(!h)break;
                            else e=e+1|0
                        }
                    }
                    e=(c[g>>2]|0)+4|0;
                    c[g>>2]=e;
                    c:do if((e|0)==(f|0))l=f;
                    else
                    {
                        l=e;
                        while(1)
                        {
                            if(!(c[l>>2]|0))break c;
                            l=l+4|0;
                            if((l|0)==(f|0))
                            {
                                l=f;
                                break
                            }
                        }
                    }
                    while(0);
                    h=c[k>>2]|0
                }
                if((h|0)==(j|0)|(e|0)==(f|0))
                {
                    r=29;
                    break b
                }
            }
            if((r|0)==15)
            {
                e=c[g>>2]|0;
                r=29;
                break
            }
            c[k>>2]=h;
            d:do if((e|0)!=(c[g>>2]|0))do
                {
                t=c[e>>2]|0;
                l=Va(c[p>>2]|0)|0;
                h=Md(h,t,q)|0;
                if(l)Va(l|0)|0;
                if((h|0)==-1)break d;
                h=(c[k>>2]|0)+h|0;
                c[k>>2]=h;
                e=e+4|0
            }
            while((e|0)!=(c[g>>2]|0));
            while(0);
            c[g>>2]=e;
            e=2
        }
        while(0);
        if((r|0)==29)e=(e|0)!=(f|0)&1;
        i=s;
        return e|0
    }
    function dl(b,d,e,f,g,h,j,k)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;
        s=i;
        i=i+16|0;
        q=s;
        a:do if((e|0)==(f|0))l=f;
        else
        {
            l=e;
            while(1)
            {
                if(!(a[l>>0]|0))break a;
                l=l+1|0;
                if((l|0)==(f|0))
                {
                    l=f;
                    break
                }
            }
        }
        while(0);
        c[k>>2]=h;
        c[g>>2]=e;
        o=j;
        p=b+8|0;
        b:do if((h|0)==(j|0)|(e|0)==(f|0))r=29;
        else
        {
            while(1)
            {
                n=d;
                m=c[n+4>>2]|0;
                b=q;
                c[b>>2]=c[n>>2];
                c[b+4>>2]=m;
                b=l;
                m=Va(c[p>>2]|0)|0;
                n=sc(h,g,b-e|0,o-h>>2,d)|0;
                if(m)Va(m|0)|0;
                if(!n)
                {
                    e=2;
                    break b
                }
                else if((n|0)==-1)break;
                h=(c[k>>2]|0)+(n<<2)|0;
                c[k>>2]=h;
                if((h|0)==(j|0))
                {
                    r=19;
                    break
                }
                e=c[g>>2]|0;
                if((l|0)==(f|0))l=f;
                else
                {
                    l=Va(c[p>>2]|0)|0;
                    e=qc(h,e,1,d)|0;
                    if(l)Va(l|0)|0;
                    if(e)
                    {
                        e=2;
                        break b
                    }
                    c[k>>2]=(c[k>>2]|0)+4;
                    e=(c[g>>2]|0)+1|0;
                    c[g>>2]=e;
                    c:do if((e|0)==(f|0))l=f;
                    else
                    {
                        l=e;
                        while(1)
                        {
                            if(!(a[l>>0]|0))break c;
                            l=l+1|0;
                            if((l|0)==(f|0))
                            {
                                l=f;
                                break
                            }
                        }
                    }
                    while(0);
                    h=c[k>>2]|0
                }
                if((h|0)==(j|0)|(e|0)==(f|0))
                {
                    r=29;
                    break b
                }
            }
            if((r|0)==19)
            {
                e=c[g>>2]|0;
                r=29;
                break
            }
            c[k>>2]=h;
            d:do if((e|0)!=(c[g>>2]|0))
            {
                while(1)
                {
                    l=Va(c[p>>2]|0)|0;
                    h=qc(h,e,b-e|0,q)|0;
                    if(l)Va(l|0)|0;
                    if((h|0)==-1)
                    {
                        r=13;
                        break
                    }
                    else if(!h)e=e+1|0;
                    else if((h|0)==-2)
                    {
                        r=14;
                        break
                    }
                    else e=e+h|0;
                    h=(c[k>>2]|0)+4|0;
                    c[k>>2]=h;
                    if((e|0)==(c[g>>2]|0))break d
                }
                if((r|0)==13)
                {
                    c[g>>2]=e;
                    e=2;
                    break b
                }
                else if((r|0)==14)
                {
                    c[g>>2]=e;
                    e=1;
                    break b
                }
            }
            while(0);
            c[g>>2]=e;
            e=(e|0)!=(f|0)&1
        }
        while(0);
        if((r|0)==29)e=(e|0)!=(f|0)&1;
        i=s;
        return e|0
    }
    function el(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,j=0;
        j=i;
        i=i+16|0;
        h=j;
        c[g>>2]=e;
        e=Va(c[b+8>>2]|0)|0;
        b=Md(h,0,d)|0;
        if(e)Va(e|0)|0;
        if(!((b|0)==0|(b|0)==-1))
        {
            b=b+-1|0;
            if(b>>>0<=(f-(c[g>>2]|0)|0)>>>0)if(!b)h=0;
            else while(1)
                {
                e=a[h>>0]|0;
                f=c[g>>2]|0;
                c[g>>2]=f+1;
                a[f>>0]=e;
                b=b+-1|0;
                if(!b)
                {
                    h=0;
                    break
                }
                else h=h+1|0
            }
            else h=1
        }
        else h=2;
        i=j;
        return h|0
    }
    function fl(a)
    {
        a=a|0;
        var b=0,d=0;
        a=a+8|0;
        b=Va(c[a>>2]|0)|0;
        d=uc(0,0,4)|0;
        if(b)Va(b|0)|0;
        if(!d)
        {
            a=c[a>>2]|0;
            if(a)
            {
                a=Va(a|0)|0;
                if(!a)a=0;
                else
                {
                    Va(a|0)|0;
                    a=0
                }
            }
            else a=1
        }
        else a=-1;
        return a|0
    }
    function gl(a)
    {
        a=a|0;
        return 0
    }
    function hl(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,i=0,j=0,k=0;
        k=e;
        j=a+8|0;
        a:do if((d|0)==(e|0)|(f|0)==0)a=0;
        else
        {
            a=0;
            i=0;
            do
            {
                h=Va(c[j>>2]|0)|0;
                g=pc(d,k-d|0,b)|0;
                if(h)Va(h|0)|0;
                if((g|0)==-2|(g|0)==-1)break a;
                else if(!g)
                {
                    d=d+1|0;
                    g=1
                }
                else d=d+g|0;
                a=g+a|0;
                i=i+1|0
            }
            while(!((d|0)==(e|0)|i>>>0>=f>>>0))
        }
        while(0);
        return a|0
    }
    function il(a)
    {
        a=a|0;
        a=c[a+8>>2]|0;
        if(a)
        {
            a=Va(a|0)|0;
            if(!a)a=4;
            else
            {
                Va(a|0)|0;
                a=4
            }
        }
        else a=1;
        return a|0
    }
    function jl(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function kl(a,b,d,e,f,g,h,j)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0;
        a=i;
        i=i+16|0;
        k=a+4|0;
        b=a;
        c[k>>2]=d;
        c[b>>2]=g;
        h=pn(d,e,k,g,h,b,1114111,0)|0;
        c[f>>2]=c[k>>2];
        c[j>>2]=c[b>>2];
        i=a;
        return h|0
    }
    function ll(a,b,d,e,f,g,h,j)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0;
        a=i;
        i=i+16|0;
        k=a+4|0;
        b=a;
        c[k>>2]=d;
        c[b>>2]=g;
        h=qn(d,e,k,g,h,b,1114111,0)|0;
        c[f>>2]=c[k>>2];
        c[j>>2]=c[b>>2];
        i=a;
        return h|0
    }
    function ml(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        c[f>>2]=d;
        return 3
    }
    function nl(a)
    {
        a=a|0;
        return 0
    }
    function ol(a)
    {
        a=a|0;
        return 0
    }
    function pl(a,b,c,d,e)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        return rn(c,d,e,1114111,0)|0
    }
    function ql(a)
    {
        a=a|0;
        return 4
    }
    function rl(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function sl(a,b,d,e,f,g,h,j)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0;
        a=i;
        i=i+16|0;
        k=a+4|0;
        b=a;
        c[k>>2]=d;
        c[b>>2]=g;
        h=sn(d,e,k,g,h,b,1114111,0)|0;
        c[f>>2]=c[k>>2];
        c[j>>2]=c[b>>2];
        i=a;
        return h|0
    }
    function tl(a,b,d,e,f,g,h,j)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0;
        a=i;
        i=i+16|0;
        k=a+4|0;
        b=a;
        c[k>>2]=d;
        c[b>>2]=g;
        h=tn(d,e,k,g,h,b,1114111,0)|0;
        c[f>>2]=c[k>>2];
        c[j>>2]=c[b>>2];
        i=a;
        return h|0
    }
    function ul(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        c[f>>2]=d;
        return 3
    }
    function vl(a)
    {
        a=a|0;
        return 0
    }
    function wl(a)
    {
        a=a|0;
        return 0
    }
    function xl(a,b,c,d,e)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        return un(c,d,e,1114111,0)|0
    }
    function yl(a)
    {
        a=a|0;
        return 4
    }
    function zl(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Al(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Bl(b,d)
    {
        b=b|0;
        d=d|0;
        c[b+4>>2]=d+-1;
        c[b>>2]=6752;
        a[b+8>>0]=46;
        a[b+9>>0]=44;
        b=b+12|0;
        c[b>>2]=0;
        c[b+4>>2]=0;
        c[b+8>>2]=0;
        return
    }
    function Cl(a,b)
    {
        a=a|0;
        b=b|0;
        c[a+4>>2]=b+-1;
        c[a>>2]=6792;
        c[a+8>>2]=46;
        c[a+12>>2]=44;
        a=a+16|0;
        c[a>>2]=0;
        c[a+4>>2]=0;
        c[a+8>>2]=0;
        return
    }
    function Dl(a)
    {
        a=a|0;
        c[a>>2]=6752;
        Ke(a+12|0);
        return
    }
    function El(a)
    {
        a=a|0;
        Dl(a);
        Lc(a);
        return
    }
    function Fl(a)
    {
        a=a|0;
        c[a>>2]=6792;
        Ke(a+16|0);
        return
    }
    function Gl(a)
    {
        a=a|0;
        Fl(a);
        Lc(a);
        return
    }
    function Hl(b)
    {
        b=b|0;
        return a[b+8>>0]|0
    }
    function Il(a)
    {
        a=a|0;
        return c[a+8>>2]|0
    }
    function Jl(b)
    {
        b=b|0;
        return a[b+9>>0]|0
    }
    function Kl(a)
    {
        a=a|0;
        return c[a+12>>2]|0
    }
    function Ll(a,b)
    {
        a=a|0;
        b=b|0;
        He(a,b+12|0);
        return
    }
    function Ml(a,b)
    {
        a=a|0;
        b=b|0;
        He(a,b+16|0);
        return
    }
    function Nl(a,b)
    {
        a=a|0;
        b=b|0;
        Ie(a,10448,4);
        return
    }
    function Ol(a,b)
    {
        a=a|0;
        b=b|0;
        Te(a,10456,Cc(10456)|0);
        return
    }
    function Pl(a,b)
    {
        a=a|0;
        b=b|0;
        Ie(a,10480,5);
        return
    }
    function Ql(a,b)
    {
        a=a|0;
        b=b|0;
        Te(a,10488,Cc(10488)|0);
        return
    }
    function Rl(a)
    {
        a=a|0;
        a=c[a+4>>2]&74;
        if((a|0)==8)a=16;
        else if(!a)a=0;
        else if((a|0)==64)a=8;
        else a=10;
        return a|0
    }
    function Sl(b)
    {
        b=b|0;
        if((a[10512]|0)==0?(Ea(10512)|0)!=0:0)
        {
            if((a[10520]|0)==0?(Ea(10520)|0)!=0:0)
            {
                b=10528;
                do
                {
                    c[b>>2]=0;
                    c[b+4>>2]=0;
                    c[b+8>>2]=0;
                    b=b+12|0
                }
                while((b|0)!=10696);
                rb(99,0,n|0)|0;
                Na(10520)
            }
            Le(10528,10696)|0;
            Le(10540,10704)|0;
            Le(10552,10712)|0;
            Le(10564,10720)|0;
            Le(10576,10736)|0;
            Le(10588,10752)|0;
            Le(10600,10760)|0;
            Le(10612,10776)|0;
            Le(10624,10784)|0;
            Le(10636,10792)|0;
            Le(10648,10800)|0;
            Le(10660,10808)|0;
            Le(10672,10816)|0;
            Le(10684,10824)|0;
            c[2708]=10528;
            Na(10512)
        }
        return c[2708]|0
    }
    function Tl(b)
    {
        b=b|0;
        if((a[10840]|0)==0?(Ea(10840)|0)!=0:0)
        {
            if((a[10848]|0)==0?(Ea(10848)|0)!=0:0)
            {
                b=10856;
                do
                {
                    c[b>>2]=0;
                    c[b+4>>2]=0;
                    c[b+8>>2]=0;
                    b=b+12|0
                }
                while((b|0)!=11024);
                rb(100,0,n|0)|0;
                Na(10848)
            }
            We(10856,11024)|0;
            We(10868,11056)|0;
            We(10880,11088)|0;
            We(10892,11120)|0;
            We(10904,11160)|0;
            We(10916,11200)|0;
            We(10928,11232)|0;
            We(10940,11272)|0;
            We(10952,11288)|0;
            We(10964,11304)|0;
            We(10976,11320)|0;
            We(10988,11336)|0;
            We(11e3,11352)|0;
            We(11012,11368)|0;
            c[2846]=10856;
            Na(10840)
        }
        return c[2846]|0
    }
    function Ul(b)
    {
        b=b|0;
        if((a[11392]|0)==0?(Ea(11392)|0)!=0:0)
        {
            if((a[11400]|0)==0?(Ea(11400)|0)!=0:0)
            {
                b=11408;
                do
                {
                    c[b>>2]=0;
                    c[b+4>>2]=0;
                    c[b+8>>2]=0;
                    b=b+12|0
                }
                while((b|0)!=11696);
                rb(101,0,n|0)|0;
                Na(11400)
            }
            Le(11408,11696)|0;
            Le(11420,11704)|0;
            Le(11432,11720)|0;
            Le(11444,11728)|0;
            Le(11456,11736)|0;
            Le(11468,11744)|0;
            Le(11480,11752)|0;
            Le(11492,11760)|0;
            Le(11504,11768)|0;
            Le(11516,11784)|0;
            Le(11528,11792)|0;
            Le(11540,11808)|0;
            Le(11552,11824)|0;
            Le(11564,11832)|0;
            Le(11576,11840)|0;
            Le(11588,11848)|0;
            Le(11600,11736)|0;
            Le(11612,11856)|0;
            Le(11624,11864)|0;
            Le(11636,11872)|0;
            Le(11648,11880)|0;
            Le(11660,11888)|0;
            Le(11672,11896)|0;
            Le(11684,11904)|0;
            c[2978]=11408;
            Na(11392)
        }
        return c[2978]|0
    }
    function Vl(b)
    {
        b=b|0;
        if((a[11920]|0)==0?(Ea(11920)|0)!=0:0)
        {
            if((a[11928]|0)==0?(Ea(11928)|0)!=0:0)
            {
                b=11936;
                do
                {
                    c[b>>2]=0;
                    c[b+4>>2]=0;
                    c[b+8>>2]=0;
                    b=b+12|0
                }
                while((b|0)!=12224);
                rb(102,0,n|0)|0;
                Na(11928)
            }
            We(11936,12224)|0;
            We(11948,12256)|0;
            We(11960,12296)|0;
            We(11972,12320)|0;
            We(11984,12344)|0;
            We(11996,12360)|0;
            We(12008,12384)|0;
            We(12020,12408)|0;
            We(12032,12440)|0;
            We(12044,12480)|0;
            We(12056,12512)|0;
            We(12068,12552)|0;
            We(12080,12592)|0;
            We(12092,12608)|0;
            We(12104,12624)|0;
            We(12116,12640)|0;
            We(12128,12344)|0;
            We(12140,12656)|0;
            We(12152,12672)|0;
            We(12164,12688)|0;
            We(12176,12704)|0;
            We(12188,12720)|0;
            We(12200,12736)|0;
            We(12212,12752)|0;
            c[3192]=11936;
            Na(11920)
        }
        return c[3192]|0
    }
    function Wl(b)
    {
        b=b|0;
        if((a[12776]|0)==0?(Ea(12776)|0)!=0:0)
        {
            if((a[12784]|0)==0?(Ea(12784)|0)!=0:0)
            {
                b=12792;
                do
                {
                    c[b>>2]=0;
                    c[b+4>>2]=0;
                    c[b+8>>2]=0;
                    b=b+12|0
                }
                while((b|0)!=13080);
                rb(103,0,n|0)|0;
                Na(12784)
            }
            Le(12792,13080)|0;
            Le(12804,13088)|0;
            c[3274]=12792;
            Na(12776)
        }
        return c[3274]|0
    }
    function Xl(b)
    {
        b=b|0;
        if((a[13104]|0)==0?(Ea(13104)|0)!=0:0)
        {
            if((a[13112]|0)==0?(Ea(13112)|0)!=0:0)
            {
                b=13120;
                do
                {
                    c[b>>2]=0;
                    c[b+4>>2]=0;
                    c[b+8>>2]=0;
                    b=b+12|0
                }
                while((b|0)!=13408);
                rb(104,0,n|0)|0;
                Na(13112)
            }
            We(13120,13408)|0;
            We(13132,13424)|0;
            c[3360]=13120;
            Na(13104)
        }
        return c[3360]|0
    }
    function Yl(b)
    {
        b=b|0;
        if((a[13448]|0)==0?(Ea(13448)|0)!=0:0)
        {
            Ie(13456,13472,8);
            rb(105,13456,n|0)|0;
            Na(13448)
        }
        return 13456
    }
    function Zl(b)
    {
        b=b|0;
        if((a[13488]|0)==0?(Ea(13488)|0)!=0:0)
        {
            Te(13536,13496,Cc(13496)|0);
            rb(106,13536,n|0)|0;
            Na(13488)
        }
        return 13536
    }
    function _l(b)
    {
        b=b|0;
        if((a[13552]|0)==0?(Ea(13552)|0)!=0:0)
        {
            Ie(13560,13576,8);
            rb(105,13560,n|0)|0;
            Na(13552)
        }
        return 13560
    }
    function $l(b)
    {
        b=b|0;
        if((a[13592]|0)==0?(Ea(13592)|0)!=0:0)
        {
            Te(13640,13600,Cc(13600)|0);
            rb(106,13640,n|0)|0;
            Na(13592)
        }
        return 13640
    }
    function am(b)
    {
        b=b|0;
        if((a[13656]|0)==0?(Ea(13656)|0)!=0:0)
        {
            Ie(13664,13680,20);
            rb(105,13664,n|0)|0;
            Na(13656)
        }
        return 13664
    }
    function bm(b)
    {
        b=b|0;
        if((a[13704]|0)==0?(Ea(13704)|0)!=0:0)
        {
            Te(13800,13712,Cc(13712)|0);
            rb(106,13800,n|0)|0;
            Na(13704)
        }
        return 13800
    }
    function cm(b)
    {
        b=b|0;
        if((a[13816]|0)==0?(Ea(13816)|0)!=0:0)
        {
            Ie(13824,13840,11);
            rb(105,13824,n|0)|0;
            Na(13816)
        }
        return 13824
    }
    function dm(b)
    {
        b=b|0;
        if((a[13856]|0)==0?(Ea(13856)|0)!=0:0)
        {
            Te(13912,13864,Cc(13864)|0);
            rb(106,13912,n|0)|0;
            Na(13856)
        }
        return 13912
    }
    function em(a)
    {
        a=a|0;
        var b=0;
        c[a>>2]=6672;
        a=a+8|0;
        b=c[a>>2]|0;
        if((b|0)!=(Jg()|0))bb(c[a>>2]|0);
        return
    }
    function fm(b,e,f,g,h,j,k)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0;
        z=i;
        i=i+112|0;
        m=z;
        o=(g-f|0)/12|0;
        if(o>>>0>100)
        {
            m=$d(o)|0;
            if(!m)sd();
            else
            {
                x=m;
                l=m
            }
        }
        else
        {
            x=0;
            l=m
        }
        if((f|0)==(g|0))m=0;
        else
        {
            q=f;
            n=0;
            p=l;
            while(1)
            {
                m=a[q>>0]|0;
                if(!(m&1))m=(m&255)>>>1;
                else m=c[q+4>>2]|0;
                if(!m)
                {
                    a[p>>0]=2;
                    m=n+1|0;
                    o=o+-1|0
                }
                else
                {
                    a[p>>0]=1;
                    m=n
                }
                q=q+12|0;
                if((q|0)==(g|0))break;
                else
                {
                    n=m;
                    p=p+1|0
                }
            }
        }
        v=(f|0)==(g|0);
        w=(f|0)==(g|0);
        u=0;
        q=m;
        r=o;
        a:while(1)
            {
            m=c[b>>2]|0;
            do if(m)
            {
                if((c[m+12>>2]|0)==(c[m+16>>2]|0))if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1)
                {
                    c[b>>2]=0;
                    m=0;
                    break
                }
                else
                {
                    m=c[b>>2]|0;
                    break
                }
            }
            else m=0;
            while(0);
            p=(m|0)==0;
            n=c[e>>2]|0;
            if(n)
            {
                if((c[n+12>>2]|0)==(c[n+16>>2]|0)?(Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0)==-1:0)
                {
                    c[e>>2]=0;
                    n=0
                }
            }
            else n=0;
            o=(n|0)==0;
            m=c[b>>2]|0;
            if(!((r|0)!=0&(p^o)))break;
            o=c[m+12>>2]|0;
            if((o|0)==(c[m+16>>2]|0))m=Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0;
            else m=d[o>>0]|0;
            m=m&255;
            if(!k)m=Wb[c[(c[h>>2]|0)+12>>2]&15](h,m)|0;
            t=u+1|0;
            if(v)
            {
                m=0;
                p=r
            }
            else
            {
                p=0;
                s=f;
                n=r;
                r=l;
                while(1)
                {
                    do if((a[r>>0]|0)==1)
                    {
                        if(!(a[s>>0]&1))o=s+1|0;
                        else o=c[s+8>>2]|0;
                        o=a[o+u>>0]|0;
                        if(!k)o=Wb[c[(c[h>>2]|0)+12>>2]&15](h,o)|0;
                        if(m<<24>>24!=o<<24>>24)
                        {
                            a[r>>0]=0;
                            o=p;
                            n=n+-1|0;
                            break
                        }
                        o=a[s>>0]|0;
                        if(!(o&1))o=(o&255)>>>1;
                        else o=c[s+4>>2]|0;
                        if((o|0)==(t|0))
                        {
                            a[r>>0]=2;
                            o=1;
                            q=q+1|0;
                            n=n+-1|0
                        }
                        else o=1
                    }
                    else o=p;
                    while(0);
                    s=s+12|0;
                    if((s|0)==(g|0))
                    {
                        m=o;
                        p=n;
                        break
                    }
                    else
                    {
                        p=o;
                        r=r+1|0
                    }
                }
            }
            if(!m)
            {
                u=t;
                r=p;
                continue
            }
            m=c[b>>2]|0;
            o=m+12|0;
            n=c[o>>2]|0;
            if((n|0)==(c[m+16>>2]|0))Qb[c[(c[m>>2]|0)+40>>2]&63](m)|0;
            else c[o>>2]=n+1;
            if((q+p|0)>>>0<2|w)
            {
                u=t;
                r=p;
                continue
            }
            else
            {
                m=f;
                n=l
            }
            while(1)
            {
                if((a[n>>0]|0)==2)
                {
                    o=a[m>>0]|0;
                    if(!(o&1))o=(o&255)>>>1;
                    else o=c[m+4>>2]|0;
                    if((o|0)!=(t|0))
                    {
                        a[n>>0]=0;
                        q=q+-1|0
                    }
                }
                m=m+12|0;
                if((m|0)==(g|0))
                {
                    u=t;
                    r=p;
                    continue a
                }
                else n=n+1|0
            }
        }
        do if(m)
        {
            if((c[m+12>>2]|0)==(c[m+16>>2]|0))if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1)
            {
                c[b>>2]=0;
                m=0;
                break
            }
            else
            {
                m=c[b>>2]|0;
                break
            }
        }
        else m=0;
        while(0);
        m=(m|0)==0;
        do if(!o)
        {
            if((c[n+12>>2]|0)==(c[n+16>>2]|0)?(Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0)==-1:0)
            {
                c[e>>2]=0;
                y=65;
                break
            }
            if(!m)y=66
        }
        else y=65;
        while(0);
        if((y|0)==65?m:0)y=66;
        if((y|0)==66)c[j>>2]=c[j>>2]|2;
        b:do if((f|0)==(g|0))y=70;
        else while(1)
            {
            if((a[l>>0]|0)==2)
            {
                g=f;
                break b
            }
            f=f+12|0;
            if((f|0)==(g|0))
            {
                y=70;
                break
            }
            else l=l+1|0
        }
        while(0);
        if((y|0)==70)c[j>>2]=c[j>>2]|4;
        ae(x);
        i=z;
        return g|0
    }
    function gm(b,e,f,g,h,j)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0;
        A=i;
        i=i+224|0;
        s=A+198|0;
        k=A+196|0;
        z=A+4|0;
        y=A+16|0;
        w=A+28|0;
        x=A+32|0;
        u=A;
        t=A+192|0;
        v=Rl(g)|0;
        Yg(z,g,s,k);
        c[y>>2]=0;
        c[y+4>>2]=0;
        c[y+8>>2]=0;
        if(!(a[y>>0]&1))b=10;
        else b=(c[y>>2]&-2)+-1|0;
        Ne(y,b,0);
        p=y+8|0;
        q=y+1|0;
        m=(a[y>>0]&1)==0?q:c[p>>2]|0;
        c[w>>2]=m;
        c[u>>2]=x;
        c[t>>2]=0;
        r=y+4|0;
        o=a[k>>0]|0;
        b=c[e>>2]|0;
        k=m;
        a:while(1)
            {
            if(b)
            {
                if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
                {
                    c[e>>2]=0;
                    b=0
                }
            }
            else b=0;
            g=(b|0)==0;
            l=c[f>>2]|0;
            do if(l)
            {
                if((c[l+12>>2]|0)!=(c[l+16>>2]|0))if(g)break;
                else break a;
                if((Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)!=-1)if(g)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    B=13;
                    break
                }
            }
            else B=13;
            while(0);
            if((B|0)==13)
            {
                B=0;
                if(g)
                {
                    l=0;
                    break
                }
                else l=0
            }
            m=a[y>>0]|0;
            m=(m&1)==0?(m&255)>>>1:c[r>>2]|0;
            if((c[w>>2]|0)==(k+m|0))
            {
                Ne(y,m<<1,0);
                if(!(a[y>>0]&1))g=10;
                else g=(c[y>>2]&-2)+-1|0;
                Ne(y,g,0);
                k=(a[y>>0]&1)==0?q:c[p>>2]|0;
                c[w>>2]=k+m
            }
            m=b+12|0;
            g=c[m>>2]|0;
            n=b+16|0;
            if((g|0)==(c[n>>2]|0))g=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else g=d[g>>0]|0;
            if(Ig(g&255,v,k,w,t,o,z,x,u,s)|0)break;
            l=c[m>>2]|0;
            if((l|0)==(c[n>>2]|0))
            {
                Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                continue
            }
            else
            {
                c[m>>2]=l+1;
                continue
            }
        }
        s=a[z>>0]|0;
        g=c[u>>2]|0;
        if((((s&1)==0?(s&255)>>>1:c[z+4>>2]|0)|0)!=0?(g-x|0)<160:0)
        {
            s=c[t>>2]|0;
            t=g+4|0;
            c[u>>2]=t;
            c[g>>2]=s;
            g=t
        }
        c[j>>2]=Pn(k,c[w>>2]|0,h,v)|0;
        Gj(z,x,g,h);
        if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
            {
                c[e>>2]=0;
                b=0
            }
        }
        else b=0;
        b=(b|0)==0;
        do if(l)
        {
            if((c[l+12>>2]|0)==(c[l+16>>2]|0)?(Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)==-1:0)
            {
                c[f>>2]=0;
                B=38;
                break
            }
            if(!b)B=39
        }
        else B=38;
        while(0);
        if((B|0)==38?b:0)B=39;
        if((B|0)==39)c[h>>2]=c[h>>2]|2;
        B=c[e>>2]|0;
        Ke(y);
        Ke(z);
        i=A;
        return B|0
    }
    function hm(b,e,f,g,h,j)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0;
        A=i;
        i=i+224|0;
        s=A+198|0;
        k=A+196|0;
        z=A+4|0;
        y=A+16|0;
        w=A+28|0;
        x=A+32|0;
        u=A;
        t=A+192|0;
        v=Rl(g)|0;
        Yg(z,g,s,k);
        c[y>>2]=0;
        c[y+4>>2]=0;
        c[y+8>>2]=0;
        if(!(a[y>>0]&1))b=10;
        else b=(c[y>>2]&-2)+-1|0;
        Ne(y,b,0);
        p=y+8|0;
        q=y+1|0;
        m=(a[y>>0]&1)==0?q:c[p>>2]|0;
        c[w>>2]=m;
        c[u>>2]=x;
        c[t>>2]=0;
        r=y+4|0;
        o=a[k>>0]|0;
        b=c[e>>2]|0;
        k=m;
        a:while(1)
            {
            if(b)
            {
                if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
                {
                    c[e>>2]=0;
                    b=0
                }
            }
            else b=0;
            g=(b|0)==0;
            l=c[f>>2]|0;
            do if(l)
            {
                if((c[l+12>>2]|0)!=(c[l+16>>2]|0))if(g)break;
                else break a;
                if((Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)!=-1)if(g)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    B=13;
                    break
                }
            }
            else B=13;
            while(0);
            if((B|0)==13)
            {
                B=0;
                if(g)
                {
                    l=0;
                    break
                }
                else l=0
            }
            m=a[y>>0]|0;
            m=(m&1)==0?(m&255)>>>1:c[r>>2]|0;
            if((c[w>>2]|0)==(k+m|0))
            {
                Ne(y,m<<1,0);
                if(!(a[y>>0]&1))g=10;
                else g=(c[y>>2]&-2)+-1|0;
                Ne(y,g,0);
                k=(a[y>>0]&1)==0?q:c[p>>2]|0;
                c[w>>2]=k+m
            }
            m=b+12|0;
            g=c[m>>2]|0;
            n=b+16|0;
            if((g|0)==(c[n>>2]|0))g=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else g=d[g>>0]|0;
            if(Ig(g&255,v,k,w,t,o,z,x,u,s)|0)break;
            l=c[m>>2]|0;
            if((l|0)==(c[n>>2]|0))
            {
                Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                continue
            }
            else
            {
                c[m>>2]=l+1;
                continue
            }
        }
        s=a[z>>0]|0;
        g=c[u>>2]|0;
        if((((s&1)==0?(s&255)>>>1:c[z+4>>2]|0)|0)!=0?(g-x|0)<160:0)
        {
            s=c[t>>2]|0;
            t=g+4|0;
            c[u>>2]=t;
            c[g>>2]=s;
            g=t
        }
        w=On(k,c[w>>2]|0,h,v)|0;
        c[j>>2]=w;
        c[j+4>>2]=G;
        Gj(z,x,g,h);
        if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
            {
                c[e>>2]=0;
                b=0
            }
        }
        else b=0;
        b=(b|0)==0;
        do if(l)
        {
            if((c[l+12>>2]|0)==(c[l+16>>2]|0)?(Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)==-1:0)
            {
                c[f>>2]=0;
                B=38;
                break
            }
            if(!b)B=39
        }
        else B=38;
        while(0);
        if((B|0)==38?b:0)B=39;
        if((B|0)==39)c[h>>2]=c[h>>2]|2;
        B=c[e>>2]|0;
        Ke(y);
        Ke(z);
        i=A;
        return B|0
    }
    function im(e,f,g,h,j,k)
    {
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0;
        B=i;
        i=i+224|0;
        t=B+198|0;
        l=B+196|0;
        A=B+4|0;
        z=B+16|0;
        x=B+28|0;
        y=B+32|0;
        v=B;
        u=B+192|0;
        w=Rl(h)|0;
        Yg(A,h,t,l);
        c[z>>2]=0;
        c[z+4>>2]=0;
        c[z+8>>2]=0;
        if(!(a[z>>0]&1))e=10;
        else e=(c[z>>2]&-2)+-1|0;
        Ne(z,e,0);
        q=z+8|0;
        r=z+1|0;
        n=(a[z>>0]&1)==0?r:c[q>>2]|0;
        c[x>>2]=n;
        c[v>>2]=y;
        c[u>>2]=0;
        s=z+4|0;
        p=a[l>>0]|0;
        e=c[f>>2]|0;
        l=n;
        a:while(1)
            {
            if(e)
            {
                if((c[e+12>>2]|0)==(c[e+16>>2]|0)?(Qb[c[(c[e>>2]|0)+36>>2]&63](e)|0)==-1:0)
                {
                    c[f>>2]=0;
                    e=0
                }
            }
            else e=0;
            h=(e|0)==0;
            m=c[g>>2]|0;
            do if(m)
            {
                if((c[m+12>>2]|0)!=(c[m+16>>2]|0))if(h)break;
                else break a;
                if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)!=-1)if(h)break;
                else break a;
                else
                {
                    c[g>>2]=0;
                    C=13;
                    break
                }
            }
            else C=13;
            while(0);
            if((C|0)==13)
            {
                C=0;
                if(h)
                {
                    m=0;
                    break
                }
                else m=0
            }
            n=a[z>>0]|0;
            n=(n&1)==0?(n&255)>>>1:c[s>>2]|0;
            if((c[x>>2]|0)==(l+n|0))
            {
                Ne(z,n<<1,0);
                if(!(a[z>>0]&1))h=10;
                else h=(c[z>>2]&-2)+-1|0;
                Ne(z,h,0);
                l=(a[z>>0]&1)==0?r:c[q>>2]|0;
                c[x>>2]=l+n
            }
            n=e+12|0;
            h=c[n>>2]|0;
            o=e+16|0;
            if((h|0)==(c[o>>2]|0))h=Qb[c[(c[e>>2]|0)+36>>2]&63](e)|0;
            else h=d[h>>0]|0;
            if(Ig(h&255,w,l,x,u,p,A,y,v,t)|0)break;
            m=c[n>>2]|0;
            if((m|0)==(c[o>>2]|0))
            {
                Qb[c[(c[e>>2]|0)+40>>2]&63](e)|0;
                continue
            }
            else
            {
                c[n>>2]=m+1;
                continue
            }
        }
        t=a[A>>0]|0;
        h=c[v>>2]|0;
        if((((t&1)==0?(t&255)>>>1:c[A+4>>2]|0)|0)!=0?(h-y|0)<160:0)
        {
            t=c[u>>2]|0;
            u=h+4|0;
            c[v>>2]=u;
            c[h>>2]=t;
            h=u
        }
        b[k>>1]=Nn(l,c[x>>2]|0,j,w)|0;
        Gj(A,y,h,j);
        if(e)
        {
            if((c[e+12>>2]|0)==(c[e+16>>2]|0)?(Qb[c[(c[e>>2]|0)+36>>2]&63](e)|0)==-1:0)
            {
                c[f>>2]=0;
                e=0
            }
        }
        else e=0;
        e=(e|0)==0;
        do if(m)
        {
            if((c[m+12>>2]|0)==(c[m+16>>2]|0)?(Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1:0)
            {
                c[g>>2]=0;
                C=38;
                break
            }
            if(!e)C=39
        }
        else C=38;
        while(0);
        if((C|0)==38?e:0)C=39;
        if((C|0)==39)c[j>>2]=c[j>>2]|2;
        C=c[f>>2]|0;
        Ke(z);
        Ke(A);
        i=B;
        return C|0
    }
    function jm(b,e,f,g,h,j)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0;
        A=i;
        i=i+224|0;
        s=A+198|0;
        k=A+196|0;
        z=A+4|0;
        y=A+16|0;
        w=A+28|0;
        x=A+32|0;
        u=A;
        t=A+192|0;
        v=Rl(g)|0;
        Yg(z,g,s,k);
        c[y>>2]=0;
        c[y+4>>2]=0;
        c[y+8>>2]=0;
        if(!(a[y>>0]&1))b=10;
        else b=(c[y>>2]&-2)+-1|0;
        Ne(y,b,0);
        p=y+8|0;
        q=y+1|0;
        m=(a[y>>0]&1)==0?q:c[p>>2]|0;
        c[w>>2]=m;
        c[u>>2]=x;
        c[t>>2]=0;
        r=y+4|0;
        o=a[k>>0]|0;
        b=c[e>>2]|0;
        k=m;
        a:while(1)
            {
            if(b)
            {
                if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
                {
                    c[e>>2]=0;
                    b=0
                }
            }
            else b=0;
            g=(b|0)==0;
            l=c[f>>2]|0;
            do if(l)
            {
                if((c[l+12>>2]|0)!=(c[l+16>>2]|0))if(g)break;
                else break a;
                if((Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)!=-1)if(g)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    B=13;
                    break
                }
            }
            else B=13;
            while(0);
            if((B|0)==13)
            {
                B=0;
                if(g)
                {
                    l=0;
                    break
                }
                else l=0
            }
            m=a[y>>0]|0;
            m=(m&1)==0?(m&255)>>>1:c[r>>2]|0;
            if((c[w>>2]|0)==(k+m|0))
            {
                Ne(y,m<<1,0);
                if(!(a[y>>0]&1))g=10;
                else g=(c[y>>2]&-2)+-1|0;
                Ne(y,g,0);
                k=(a[y>>0]&1)==0?q:c[p>>2]|0;
                c[w>>2]=k+m
            }
            m=b+12|0;
            g=c[m>>2]|0;
            n=b+16|0;
            if((g|0)==(c[n>>2]|0))g=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else g=d[g>>0]|0;
            if(Ig(g&255,v,k,w,t,o,z,x,u,s)|0)break;
            l=c[m>>2]|0;
            if((l|0)==(c[n>>2]|0))
            {
                Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                continue
            }
            else
            {
                c[m>>2]=l+1;
                continue
            }
        }
        s=a[z>>0]|0;
        g=c[u>>2]|0;
        if((((s&1)==0?(s&255)>>>1:c[z+4>>2]|0)|0)!=0?(g-x|0)<160:0)
        {
            s=c[t>>2]|0;
            t=g+4|0;
            c[u>>2]=t;
            c[g>>2]=s;
            g=t
        }
        c[j>>2]=Mn(k,c[w>>2]|0,h,v)|0;
        Gj(z,x,g,h);
        if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
            {
                c[e>>2]=0;
                b=0
            }
        }
        else b=0;
        b=(b|0)==0;
        do if(l)
        {
            if((c[l+12>>2]|0)==(c[l+16>>2]|0)?(Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)==-1:0)
            {
                c[f>>2]=0;
                B=38;
                break
            }
            if(!b)B=39
        }
        else B=38;
        while(0);
        if((B|0)==38?b:0)B=39;
        if((B|0)==39)c[h>>2]=c[h>>2]|2;
        B=c[e>>2]|0;
        Ke(y);
        Ke(z);
        i=A;
        return B|0
    }
    function km(b,e,f,g,h,j)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0;
        A=i;
        i=i+224|0;
        s=A+198|0;
        k=A+196|0;
        z=A+4|0;
        y=A+16|0;
        w=A+28|0;
        x=A+32|0;
        u=A;
        t=A+192|0;
        v=Rl(g)|0;
        Yg(z,g,s,k);
        c[y>>2]=0;
        c[y+4>>2]=0;
        c[y+8>>2]=0;
        if(!(a[y>>0]&1))b=10;
        else b=(c[y>>2]&-2)+-1|0;
        Ne(y,b,0);
        p=y+8|0;
        q=y+1|0;
        m=(a[y>>0]&1)==0?q:c[p>>2]|0;
        c[w>>2]=m;
        c[u>>2]=x;
        c[t>>2]=0;
        r=y+4|0;
        o=a[k>>0]|0;
        b=c[e>>2]|0;
        k=m;
        a:while(1)
            {
            if(b)
            {
                if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
                {
                    c[e>>2]=0;
                    b=0
                }
            }
            else b=0;
            g=(b|0)==0;
            l=c[f>>2]|0;
            do if(l)
            {
                if((c[l+12>>2]|0)!=(c[l+16>>2]|0))if(g)break;
                else break a;
                if((Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)!=-1)if(g)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    B=13;
                    break
                }
            }
            else B=13;
            while(0);
            if((B|0)==13)
            {
                B=0;
                if(g)
                {
                    l=0;
                    break
                }
                else l=0
            }
            m=a[y>>0]|0;
            m=(m&1)==0?(m&255)>>>1:c[r>>2]|0;
            if((c[w>>2]|0)==(k+m|0))
            {
                Ne(y,m<<1,0);
                if(!(a[y>>0]&1))g=10;
                else g=(c[y>>2]&-2)+-1|0;
                Ne(y,g,0);
                k=(a[y>>0]&1)==0?q:c[p>>2]|0;
                c[w>>2]=k+m
            }
            m=b+12|0;
            g=c[m>>2]|0;
            n=b+16|0;
            if((g|0)==(c[n>>2]|0))g=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else g=d[g>>0]|0;
            if(Ig(g&255,v,k,w,t,o,z,x,u,s)|0)break;
            l=c[m>>2]|0;
            if((l|0)==(c[n>>2]|0))
            {
                Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                continue
            }
            else
            {
                c[m>>2]=l+1;
                continue
            }
        }
        s=a[z>>0]|0;
        g=c[u>>2]|0;
        if((((s&1)==0?(s&255)>>>1:c[z+4>>2]|0)|0)!=0?(g-x|0)<160:0)
        {
            s=c[t>>2]|0;
            t=g+4|0;
            c[u>>2]=t;
            c[g>>2]=s;
            g=t
        }
        c[j>>2]=Ln(k,c[w>>2]|0,h,v)|0;
        Gj(z,x,g,h);
        if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
            {
                c[e>>2]=0;
                b=0
            }
        }
        else b=0;
        b=(b|0)==0;
        do if(l)
        {
            if((c[l+12>>2]|0)==(c[l+16>>2]|0)?(Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)==-1:0)
            {
                c[f>>2]=0;
                B=38;
                break
            }
            if(!b)B=39
        }
        else B=38;
        while(0);
        if((B|0)==38?b:0)B=39;
        if((B|0)==39)c[h>>2]=c[h>>2]|2;
        B=c[e>>2]|0;
        Ke(y);
        Ke(z);
        i=A;
        return B|0
    }
    function lm(b,e,f,g,h,j)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0;
        A=i;
        i=i+224|0;
        s=A+198|0;
        k=A+196|0;
        z=A+4|0;
        y=A+16|0;
        w=A+28|0;
        x=A+32|0;
        u=A;
        t=A+192|0;
        v=Rl(g)|0;
        Yg(z,g,s,k);
        c[y>>2]=0;
        c[y+4>>2]=0;
        c[y+8>>2]=0;
        if(!(a[y>>0]&1))b=10;
        else b=(c[y>>2]&-2)+-1|0;
        Ne(y,b,0);
        p=y+8|0;
        q=y+1|0;
        m=(a[y>>0]&1)==0?q:c[p>>2]|0;
        c[w>>2]=m;
        c[u>>2]=x;
        c[t>>2]=0;
        r=y+4|0;
        o=a[k>>0]|0;
        b=c[e>>2]|0;
        k=m;
        a:while(1)
            {
            if(b)
            {
                if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
                {
                    c[e>>2]=0;
                    b=0
                }
            }
            else b=0;
            g=(b|0)==0;
            l=c[f>>2]|0;
            do if(l)
            {
                if((c[l+12>>2]|0)!=(c[l+16>>2]|0))if(g)break;
                else break a;
                if((Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)!=-1)if(g)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    B=13;
                    break
                }
            }
            else B=13;
            while(0);
            if((B|0)==13)
            {
                B=0;
                if(g)
                {
                    l=0;
                    break
                }
                else l=0
            }
            m=a[y>>0]|0;
            m=(m&1)==0?(m&255)>>>1:c[r>>2]|0;
            if((c[w>>2]|0)==(k+m|0))
            {
                Ne(y,m<<1,0);
                if(!(a[y>>0]&1))g=10;
                else g=(c[y>>2]&-2)+-1|0;
                Ne(y,g,0);
                k=(a[y>>0]&1)==0?q:c[p>>2]|0;
                c[w>>2]=k+m
            }
            m=b+12|0;
            g=c[m>>2]|0;
            n=b+16|0;
            if((g|0)==(c[n>>2]|0))g=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else g=d[g>>0]|0;
            if(Ig(g&255,v,k,w,t,o,z,x,u,s)|0)break;
            l=c[m>>2]|0;
            if((l|0)==(c[n>>2]|0))
            {
                Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                continue
            }
            else
            {
                c[m>>2]=l+1;
                continue
            }
        }
        s=a[z>>0]|0;
        g=c[u>>2]|0;
        if((((s&1)==0?(s&255)>>>1:c[z+4>>2]|0)|0)!=0?(g-x|0)<160:0)
        {
            s=c[t>>2]|0;
            t=g+4|0;
            c[u>>2]=t;
            c[g>>2]=s;
            g=t
        }
        w=Kn(k,c[w>>2]|0,h,v)|0;
        c[j>>2]=w;
        c[j+4>>2]=G;
        Gj(z,x,g,h);
        if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
            {
                c[e>>2]=0;
                b=0
            }
        }
        else b=0;
        b=(b|0)==0;
        do if(l)
        {
            if((c[l+12>>2]|0)==(c[l+16>>2]|0)?(Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)==-1:0)
            {
                c[f>>2]=0;
                B=38;
                break
            }
            if(!b)B=39
        }
        else B=38;
        while(0);
        if((B|0)==38?b:0)B=39;
        if((B|0)==39)c[h>>2]=c[h>>2]|2;
        B=c[e>>2]|0;
        Ke(y);
        Ke(z);
        i=A;
        return B|0
    }
    function mm(b,e,f,h,j,k)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        h=h|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0;
        D=i;
        i=i+240|0;
        u=D+200|0;
        m=D+199|0;
        l=D+198|0;
        C=D+8|0;
        B=D+20|0;
        z=D+192|0;
        A=D+32|0;
        x=D;
        w=D+4|0;
        y=D+197|0;
        v=D+196|0;
        Zg(C,h,u,m,l);
        c[B>>2]=0;
        c[B+4>>2]=0;
        c[B+8>>2]=0;
        if(!(a[B>>0]&1))b=10;
        else b=(c[B>>2]&-2)+-1|0;
        Ne(B,b,0);
        r=B+8|0;
        s=B+1|0;
        n=(a[B>>0]&1)==0?s:c[r>>2]|0;
        c[z>>2]=n;
        c[x>>2]=A;
        c[w>>2]=0;
        a[y>>0]=1;
        a[v>>0]=69;
        t=B+4|0;
        q=a[m>>0]|0;
        p=a[l>>0]|0;
        b=c[e>>2]|0;
        l=n;
        a:while(1)
            {
            if(b)
            {
                if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
                {
                    c[e>>2]=0;
                    b=0
                }
            }
            else b=0;
            h=(b|0)==0;
            m=c[f>>2]|0;
            do if(m)
            {
                if((c[m+12>>2]|0)!=(c[m+16>>2]|0))if(h)break;
                else break a;
                if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)!=-1)if(h)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    E=13;
                    break
                }
            }
            else E=13;
            while(0);
            if((E|0)==13)
            {
                E=0;
                if(h)
                {
                    m=0;
                    break
                }
                else m=0
            }
            n=a[B>>0]|0;
            n=(n&1)==0?(n&255)>>>1:c[t>>2]|0;
            if((c[z>>2]|0)==(l+n|0))
            {
                Ne(B,n<<1,0);
                if(!(a[B>>0]&1))h=10;
                else h=(c[B>>2]&-2)+-1|0;
                Ne(B,h,0);
                l=(a[B>>0]&1)==0?s:c[r>>2]|0;
                c[z>>2]=l+n
            }
            n=b+12|0;
            h=c[n>>2]|0;
            o=b+16|0;
            if((h|0)==(c[o>>2]|0))h=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else h=d[h>>0]|0;
            if(_g(h&255,y,v,l,z,q,p,C,A,x,w,u)|0)break;
            m=c[n>>2]|0;
            if((m|0)==(c[o>>2]|0))
            {
                Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                continue
            }
            else
            {
                c[n>>2]=m+1;
                continue
            }
        }
        v=a[C>>0]|0;
        h=c[x>>2]|0;
        if(!((a[y>>0]|0)==0?1:(((v&1)==0?(v&255)>>>1:c[C+4>>2]|0)|0)==0)?(h-A|0)<160:0)
        {
            w=c[w>>2]|0;
            y=h+4|0;
            c[x>>2]=y;
            c[h>>2]=w;
            h=y
        }
        g[k>>2]=+Jn(l,c[z>>2]|0,j);
        Gj(C,A,h,j);
        if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
            {
                c[e>>2]=0;
                b=0
            }
        }
        else b=0;
        b=(b|0)==0;
        do if(m)
        {
            if((c[m+12>>2]|0)==(c[m+16>>2]|0)?(Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1:0)
            {
                c[f>>2]=0;
                E=38;
                break
            }
            if(!b)E=39
        }
        else E=38;
        while(0);
        if((E|0)==38?b:0)E=39;
        if((E|0)==39)c[j>>2]=c[j>>2]|2;
        E=c[e>>2]|0;
        Ke(B);
        Ke(C);
        i=D;
        return E|0
    }
    function nm(b,e,f,g,j,k)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0;
        D=i;
        i=i+240|0;
        u=D+200|0;
        m=D+199|0;
        l=D+198|0;
        C=D+8|0;
        B=D+20|0;
        z=D+192|0;
        A=D+32|0;
        x=D;
        w=D+4|0;
        y=D+197|0;
        v=D+196|0;
        Zg(C,g,u,m,l);
        c[B>>2]=0;
        c[B+4>>2]=0;
        c[B+8>>2]=0;
        if(!(a[B>>0]&1))b=10;
        else b=(c[B>>2]&-2)+-1|0;
        Ne(B,b,0);
        r=B+8|0;
        s=B+1|0;
        n=(a[B>>0]&1)==0?s:c[r>>2]|0;
        c[z>>2]=n;
        c[x>>2]=A;
        c[w>>2]=0;
        a[y>>0]=1;
        a[v>>0]=69;
        t=B+4|0;
        q=a[m>>0]|0;
        p=a[l>>0]|0;
        b=c[e>>2]|0;
        l=n;
        a:while(1)
            {
            if(b)
            {
                if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
                {
                    c[e>>2]=0;
                    b=0
                }
            }
            else b=0;
            g=(b|0)==0;
            m=c[f>>2]|0;
            do if(m)
            {
                if((c[m+12>>2]|0)!=(c[m+16>>2]|0))if(g)break;
                else break a;
                if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)!=-1)if(g)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    E=13;
                    break
                }
            }
            else E=13;
            while(0);
            if((E|0)==13)
            {
                E=0;
                if(g)
                {
                    m=0;
                    break
                }
                else m=0
            }
            n=a[B>>0]|0;
            n=(n&1)==0?(n&255)>>>1:c[t>>2]|0;
            if((c[z>>2]|0)==(l+n|0))
            {
                Ne(B,n<<1,0);
                if(!(a[B>>0]&1))g=10;
                else g=(c[B>>2]&-2)+-1|0;
                Ne(B,g,0);
                l=(a[B>>0]&1)==0?s:c[r>>2]|0;
                c[z>>2]=l+n
            }
            n=b+12|0;
            g=c[n>>2]|0;
            o=b+16|0;
            if((g|0)==(c[o>>2]|0))g=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else g=d[g>>0]|0;
            if(_g(g&255,y,v,l,z,q,p,C,A,x,w,u)|0)break;
            m=c[n>>2]|0;
            if((m|0)==(c[o>>2]|0))
            {
                Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                continue
            }
            else
            {
                c[n>>2]=m+1;
                continue
            }
        }
        v=a[C>>0]|0;
        g=c[x>>2]|0;
        if(!((a[y>>0]|0)==0?1:(((v&1)==0?(v&255)>>>1:c[C+4>>2]|0)|0)==0)?(g-A|0)<160:0)
        {
            w=c[w>>2]|0;
            y=g+4|0;
            c[x>>2]=y;
            c[g>>2]=w;
            g=y
        }
        h[k>>3]=+In(l,c[z>>2]|0,j);
        Gj(C,A,g,j);
        if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
            {
                c[e>>2]=0;
                b=0
            }
        }
        else b=0;
        b=(b|0)==0;
        do if(m)
        {
            if((c[m+12>>2]|0)==(c[m+16>>2]|0)?(Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1:0)
            {
                c[f>>2]=0;
                E=38;
                break
            }
            if(!b)E=39
        }
        else E=38;
        while(0);
        if((E|0)==38?b:0)E=39;
        if((E|0)==39)c[j>>2]=c[j>>2]|2;
        E=c[e>>2]|0;
        Ke(B);
        Ke(C);
        i=D;
        return E|0
    }
    function om(b,e,f,g,j,k)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0;
        D=i;
        i=i+240|0;
        u=D+200|0;
        m=D+199|0;
        l=D+198|0;
        C=D+8|0;
        B=D+20|0;
        z=D+192|0;
        A=D+32|0;
        x=D;
        w=D+4|0;
        y=D+197|0;
        v=D+196|0;
        Zg(C,g,u,m,l);
        c[B>>2]=0;
        c[B+4>>2]=0;
        c[B+8>>2]=0;
        if(!(a[B>>0]&1))b=10;
        else b=(c[B>>2]&-2)+-1|0;
        Ne(B,b,0);
        r=B+8|0;
        s=B+1|0;
        n=(a[B>>0]&1)==0?s:c[r>>2]|0;
        c[z>>2]=n;
        c[x>>2]=A;
        c[w>>2]=0;
        a[y>>0]=1;
        a[v>>0]=69;
        t=B+4|0;
        q=a[m>>0]|0;
        p=a[l>>0]|0;
        b=c[e>>2]|0;
        l=n;
        a:while(1)
            {
            if(b)
            {
                if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
                {
                    c[e>>2]=0;
                    b=0
                }
            }
            else b=0;
            g=(b|0)==0;
            m=c[f>>2]|0;
            do if(m)
            {
                if((c[m+12>>2]|0)!=(c[m+16>>2]|0))if(g)break;
                else break a;
                if((Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)!=-1)if(g)break;
                else break a;
                else
                {
                    c[f>>2]=0;
                    E=13;
                    break
                }
            }
            else E=13;
            while(0);
            if((E|0)==13)
            {
                E=0;
                if(g)
                {
                    m=0;
                    break
                }
                else m=0
            }
            n=a[B>>0]|0;
            n=(n&1)==0?(n&255)>>>1:c[t>>2]|0;
            if((c[z>>2]|0)==(l+n|0))
            {
                Ne(B,n<<1,0);
                if(!(a[B>>0]&1))g=10;
                else g=(c[B>>2]&-2)+-1|0;
                Ne(B,g,0);
                l=(a[B>>0]&1)==0?s:c[r>>2]|0;
                c[z>>2]=l+n
            }
            n=b+12|0;
            g=c[n>>2]|0;
            o=b+16|0;
            if((g|0)==(c[o>>2]|0))g=Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0;
            else g=d[g>>0]|0;
            if(_g(g&255,y,v,l,z,q,p,C,A,x,w,u)|0)break;
            m=c[n>>2]|0;
            if((m|0)==(c[o>>2]|0))
            {
                Qb[c[(c[b>>2]|0)+40>>2]&63](b)|0;
                continue
            }
            else
            {
                c[n>>2]=m+1;
                continue
            }
        }
        v=a[C>>0]|0;
        g=c[x>>2]|0;
        if(!((a[y>>0]|0)==0?1:(((v&1)==0?(v&255)>>>1:c[C+4>>2]|0)|0)==0)?(g-A|0)<160:0)
        {
            w=c[w>>2]|0;
            y=g+4|0;
            c[x>>2]=y;
            c[g>>2]=w;
            g=y
        }
        h[k>>3]=+Hn(l,c[z>>2]|0,j);
        Gj(C,A,g,j);
        if(b)
        {
            if((c[b+12>>2]|0)==(c[b+16>>2]|0)?(Qb[c[(c[b>>2]|0)+36>>2]&63](b)|0)==-1:0)
            {
                c[e>>2]=0;
                b=0
            }
        }
        else b=0;
        b=(b|0)==0;
        do if(m)
        {
            if((c[m+12>>2]|0)==(c[m+16>>2]|0)?(Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0)==-1:0)
            {
                c[f>>2]=0;
                E=38;
                break
            }
            if(!b)E=39
        }
        else E=38;
        while(0);
        if((E|0)==38?b:0)E=39;
        if((E|0)==39)c[j>>2]=c[j>>2]|2;
        E=c[e>>2]|0;
        Ke(B);
        Ke(C);
        i=D;
        return E|0
    }
    function pm(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        f=i;
        i=i+16|0;
        g=f;
        c[g>>2]=e;
        e=Va(b|0)|0;
        b=Bc(a,d,g)|0;
        if(e)Va(e|0)|0;
        i=f;
        return b|0
    }
    function qm(b,d,e,f,g,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0;
        y=i;
        i=i+112|0;
        l=y;
        n=(f-e|0)/12|0;
        if(n>>>0>100)
        {
            l=$d(n)|0;
            if(!l)sd();
            else
            {
                w=l;
                k=l
            }
        }
        else
        {
            w=0;
            k=l
        }
        if((e|0)==(f|0))l=0;
        else
        {
            p=e;
            m=0;
            o=k;
            while(1)
            {
                l=a[p>>0]|0;
                if(!(l&1))l=(l&255)>>>1;
                else l=c[p+4>>2]|0;
                if(!l)
                {
                    a[o>>0]=2;
                    l=m+1|0;
                    n=n+-1|0
                }
                else
                {
                    a[o>>0]=1;
                    l=m
                }
                p=p+12|0;
                if((p|0)==(f|0))break;
                else
                {
                    m=l;
                    o=o+1|0
                }
            }
        }
        u=(e|0)==(f|0);
        v=(e|0)==(f|0);
        t=0;
        q=n;
        a:while(1)
            {
            n=c[b>>2]|0;
            do if(n)
            {
                m=c[n+12>>2]|0;
                if((m|0)==(c[n+16>>2]|0))n=Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0;
                else n=c[m>>2]|0;
                if((n|0)==-1)
                {
                    c[b>>2]=0;
                    p=1;
                    break
                }
                else
                {
                    p=(c[b>>2]|0)==0;
                    break
                }
            }
            else p=1;
            while(0);
            n=c[d>>2]|0;
            if(n)
            {
                m=c[n+12>>2]|0;
                if((m|0)==(c[n+16>>2]|0))m=Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0;
                else m=c[m>>2]|0;
                if((m|0)==-1)
                {
                    c[d>>2]=0;
                    n=0;
                    o=1
                }
                else o=0
            }
            else
            {
                n=0;
                o=1
            }
            m=c[b>>2]|0;
            if(!((q|0)!=0&(p^o)))break;
            n=c[m+12>>2]|0;
            if((n|0)==(c[m+16>>2]|0))n=Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0;
            else n=c[n>>2]|0;
            if(!j)n=Wb[c[(c[g>>2]|0)+28>>2]&15](g,n)|0;
            s=t+1|0;
            if(u)p=0;
            else
            {
                p=0;
                r=e;
                o=q;
                q=k;
                while(1)
                {
                    do if((a[q>>0]|0)==1)
                    {
                        if(!(a[r>>0]&1))m=r+4|0;
                        else m=c[r+8>>2]|0;
                        m=c[m+(t<<2)>>2]|0;
                        if(!j)m=Wb[c[(c[g>>2]|0)+28>>2]&15](g,m)|0;
                        if((n|0)!=(m|0))
                        {
                            a[q>>0]=0;
                            m=o+-1|0;
                            break
                        }
                        p=a[r>>0]|0;
                        if(!(p&1))p=(p&255)>>>1;
                        else p=c[r+4>>2]|0;
                        if((p|0)==(s|0))
                        {
                            a[q>>0]=2;
                            p=1;
                            l=l+1|0;
                            m=o+-1|0
                        }
                        else
                        {
                            p=1;
                            m=o
                        }
                    }
                    else m=o;
                    while(0);
                    r=r+12|0;
                    if((r|0)==(f|0))
                    {
                        q=m;
                        break
                    }
                    else
                    {
                        o=m;
                        q=q+1|0
                    }
                }
            }
            if(!p)
            {
                t=s;
                continue
            }
            n=c[b>>2]|0;
            m=n+12|0;
            o=c[m>>2]|0;
            if((o|0)==(c[n+16>>2]|0))Qb[c[(c[n>>2]|0)+40>>2]&63](n)|0;
            else c[m>>2]=o+4;
            if((l+q|0)>>>0<2|v)
            {
                t=s;
                continue
            }
            else
            {
                n=e;
                p=k
            }
            while(1)
            {
                if((a[p>>0]|0)==2)
                {
                    m=a[n>>0]|0;
                    if(!(m&1))m=(m&255)>>>1;
                    else m=c[n+4>>2]|0;
                    if((m|0)!=(s|0))
                    {
                        a[p>>0]=0;
                        l=l+-1|0
                    }
                }
                n=n+12|0;
                if((n|0)==(f|0))
                {
                    t=s;
                    continue a
                }
                else p=p+1|0
            }
        }
        do if(m)
        {
            l=c[m+12>>2]|0;
            if((l|0)==(c[m+16>>2]|0))l=Qb[c[(c[m>>2]|0)+36>>2]&63](m)|0;
            else l=c[l>>2]|0;
            if((l|0)==-1)
            {
                c[b>>2]=0;
                m=1;
                break
            }
            else
            {
                m=(c[b>>2]|0)==0;
                break
            }
        }
        else m=1;
        while(0);
        do if(n)
        {
            l=c[n+12>>2]|0;
            if((l|0)==(c[n+16>>2]|0))l=Qb[c[(c[n>>2]|0)+36>>2]&63](n)|0;
            else l=c[l>>2]|0;
            if((l|0)!=-1)if(m)break;
            else
            {
                x=74;
                break
            }
            else
            {
                c[d>>2]=0;
                x=72;
                break
            }
        }
        else x=72;
        while(0);
        if((x|0)==72?m:0)x=74;
        if((x|0)==74)c[h>>2]=c[h>>2]|2;
        b:do if((e|0)==(f|0))x=78;
        else while(1)
            {
            if((a[k>>0]|0)==2)
            {
                f=e;
                break b
            }
            e=e+12|0;
            if((e|0)==(f|0))
            {
                x=78;
                break
            }
            else k=k+1|0
        }
        while(0);
        if((x|0)==78)c[h>>2]=c[h>>2]|4;
        ae(w);
        i=y;
        return f|0
    }
    function rm(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;
        z=i;
        i=i+304|0;
        r=z+200|0;
        j=z;
        y=z+4|0;
        x=z+16|0;
        v=z+28|0;
        w=z+32|0;
        t=z+192|0;
        s=z+196|0;
        u=Rl(f)|0;
        $g(y,f,r,j);
        c[x>>2]=0;
        c[x+4>>2]=0;
        c[x+8>>2]=0;
        if(!(a[x>>0]&1))b=10;
        else b=(c[x>>2]&-2)+-1|0;
        Ne(x,b,0);
        o=x+8|0;
        p=x+1|0;
        f=(a[x>>0]&1)==0?p:c[o>>2]|0;
        c[v>>2]=f;
        c[t>>2]=w;
        c[s>>2]=0;
        q=x+4|0;
        n=c[j>>2]|0;
        j=c[d>>2]|0;
        a:while(1)
            {
            if(j)
            {
                b=c[j+12>>2]|0;
                if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
                else b=c[b>>2]|0;
                if((b|0)==-1)
                {
                    c[d>>2]=0;
                    j=0;
                    l=1
                }
                else l=0
            }
            else
            {
                j=0;
                l=1
            }
            k=c[e>>2]|0;
            do if(k)
            {
                b=c[k+12>>2]|0;
                if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else b=c[b>>2]|0;
                if((b|0)!=-1)if(l)
                {
                    m=k;
                    break
                }
                else break a;
                else
                {
                    c[e>>2]=0;
                    A=16;
                    break
                }
            }
            else A=16;
            while(0);
            if((A|0)==16)
            {
                A=0;
                if(l)
                {
                    k=0;
                    break
                }
                else m=0
            }
            k=a[x>>0]|0;
            k=(k&1)==0?(k&255)>>>1:c[q>>2]|0;
            if((c[v>>2]|0)==(f+k|0))
            {
                Ne(x,k<<1,0);
                if(!(a[x>>0]&1))b=10;
                else b=(c[x>>2]&-2)+-1|0;
                Ne(x,b,0);
                f=(a[x>>0]&1)==0?p:c[o>>2]|0;
                c[v>>2]=f+k
            }
            k=j+12|0;
            b=c[k>>2]|0;
            l=j+16|0;
            if((b|0)==(c[l>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if(Xg(b,u,f,v,s,n,y,w,t,r)|0)
            {
                k=m;
                break
            }
            b=c[k>>2]|0;
            if((b|0)==(c[l>>2]|0))
            {
                Qb[c[(c[j>>2]|0)+40>>2]&63](j)|0;
                continue
            }
            else
            {
                c[k>>2]=b+4;
                continue
            }
        }
        r=a[y>>0]|0;
        b=c[t>>2]|0;
        if((((r&1)==0?(r&255)>>>1:c[y+4>>2]|0)|0)!=0?(b-w|0)<160:0)
        {
            r=c[s>>2]|0;
            s=b+4|0;
            c[t>>2]=s;
            c[b>>2]=r;
            b=s
        }
        c[h>>2]=Pn(f,c[v>>2]|0,g,u)|0;
        Gj(y,w,b,g);
        if(j)
        {
            b=c[j+12>>2]|0;
            if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                f=1
            }
            else f=0
        }
        else f=1;
        do if(k)
        {
            b=c[k+12>>2]|0;
            if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(f)break;
            else
            {
                A=46;
                break
            }
            else
            {
                c[e>>2]=0;
                A=44;
                break
            }
        }
        else A=44;
        while(0);
        if((A|0)==44?f:0)A=46;
        if((A|0)==46)c[g>>2]=c[g>>2]|2;
        A=c[d>>2]|0;
        Ke(x);
        Ke(y);
        i=z;
        return A|0
    }
    function sm(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;
        z=i;
        i=i+304|0;
        r=z+200|0;
        j=z;
        y=z+4|0;
        x=z+16|0;
        v=z+28|0;
        w=z+32|0;
        t=z+192|0;
        s=z+196|0;
        u=Rl(f)|0;
        $g(y,f,r,j);
        c[x>>2]=0;
        c[x+4>>2]=0;
        c[x+8>>2]=0;
        if(!(a[x>>0]&1))b=10;
        else b=(c[x>>2]&-2)+-1|0;
        Ne(x,b,0);
        o=x+8|0;
        p=x+1|0;
        f=(a[x>>0]&1)==0?p:c[o>>2]|0;
        c[v>>2]=f;
        c[t>>2]=w;
        c[s>>2]=0;
        q=x+4|0;
        n=c[j>>2]|0;
        j=c[d>>2]|0;
        a:while(1)
            {
            if(j)
            {
                b=c[j+12>>2]|0;
                if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
                else b=c[b>>2]|0;
                if((b|0)==-1)
                {
                    c[d>>2]=0;
                    j=0;
                    l=1
                }
                else l=0
            }
            else
            {
                j=0;
                l=1
            }
            k=c[e>>2]|0;
            do if(k)
            {
                b=c[k+12>>2]|0;
                if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else b=c[b>>2]|0;
                if((b|0)!=-1)if(l)
                {
                    m=k;
                    break
                }
                else break a;
                else
                {
                    c[e>>2]=0;
                    A=16;
                    break
                }
            }
            else A=16;
            while(0);
            if((A|0)==16)
            {
                A=0;
                if(l)
                {
                    k=0;
                    break
                }
                else m=0
            }
            k=a[x>>0]|0;
            k=(k&1)==0?(k&255)>>>1:c[q>>2]|0;
            if((c[v>>2]|0)==(f+k|0))
            {
                Ne(x,k<<1,0);
                if(!(a[x>>0]&1))b=10;
                else b=(c[x>>2]&-2)+-1|0;
                Ne(x,b,0);
                f=(a[x>>0]&1)==0?p:c[o>>2]|0;
                c[v>>2]=f+k
            }
            k=j+12|0;
            b=c[k>>2]|0;
            l=j+16|0;
            if((b|0)==(c[l>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if(Xg(b,u,f,v,s,n,y,w,t,r)|0)
            {
                k=m;
                break
            }
            b=c[k>>2]|0;
            if((b|0)==(c[l>>2]|0))
            {
                Qb[c[(c[j>>2]|0)+40>>2]&63](j)|0;
                continue
            }
            else
            {
                c[k>>2]=b+4;
                continue
            }
        }
        r=a[y>>0]|0;
        b=c[t>>2]|0;
        if((((r&1)==0?(r&255)>>>1:c[y+4>>2]|0)|0)!=0?(b-w|0)<160:0)
        {
            r=c[s>>2]|0;
            s=b+4|0;
            c[t>>2]=s;
            c[b>>2]=r;
            b=s
        }
        v=On(f,c[v>>2]|0,g,u)|0;
        c[h>>2]=v;
        c[h+4>>2]=G;
        Gj(y,w,b,g);
        if(j)
        {
            b=c[j+12>>2]|0;
            if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                f=1
            }
            else f=0
        }
        else f=1;
        do if(k)
        {
            b=c[k+12>>2]|0;
            if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(f)break;
            else
            {
                A=46;
                break
            }
            else
            {
                c[e>>2]=0;
                A=44;
                break
            }
        }
        else A=44;
        while(0);
        if((A|0)==44?f:0)A=46;
        if((A|0)==46)c[g>>2]=c[g>>2]|2;
        A=c[d>>2]|0;
        Ke(x);
        Ke(y);
        i=z;
        return A|0
    }
    function tm(d,e,f,g,h,j)
    {
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0;
        A=i;
        i=i+304|0;
        s=A+200|0;
        k=A;
        z=A+4|0;
        y=A+16|0;
        w=A+28|0;
        x=A+32|0;
        u=A+192|0;
        t=A+196|0;
        v=Rl(g)|0;
        $g(z,g,s,k);
        c[y>>2]=0;
        c[y+4>>2]=0;
        c[y+8>>2]=0;
        if(!(a[y>>0]&1))d=10;
        else d=(c[y>>2]&-2)+-1|0;
        Ne(y,d,0);
        p=y+8|0;
        q=y+1|0;
        g=(a[y>>0]&1)==0?q:c[p>>2]|0;
        c[w>>2]=g;
        c[u>>2]=x;
        c[t>>2]=0;
        r=y+4|0;
        o=c[k>>2]|0;
        k=c[e>>2]|0;
        a:while(1)
            {
            if(k)
            {
                d=c[k+12>>2]|0;
                if((d|0)==(c[k+16>>2]|0))d=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else d=c[d>>2]|0;
                if((d|0)==-1)
                {
                    c[e>>2]=0;
                    k=0;
                    m=1
                }
                else m=0
            }
            else
            {
                k=0;
                m=1
            }
            l=c[f>>2]|0;
            do if(l)
            {
                d=c[l+12>>2]|0;
                if((d|0)==(c[l+16>>2]|0))d=Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0;
                else d=c[d>>2]|0;
                if((d|0)!=-1)if(m)
                {
                    n=l;
                    break
                }
                else break a;
                else
                {
                    c[f>>2]=0;
                    B=16;
                    break
                }
            }
            else B=16;
            while(0);
            if((B|0)==16)
            {
                B=0;
                if(m)
                {
                    l=0;
                    break
                }
                else n=0
            }
            l=a[y>>0]|0;
            l=(l&1)==0?(l&255)>>>1:c[r>>2]|0;
            if((c[w>>2]|0)==(g+l|0))
            {
                Ne(y,l<<1,0);
                if(!(a[y>>0]&1))d=10;
                else d=(c[y>>2]&-2)+-1|0;
                Ne(y,d,0);
                g=(a[y>>0]&1)==0?q:c[p>>2]|0;
                c[w>>2]=g+l
            }
            l=k+12|0;
            d=c[l>>2]|0;
            m=k+16|0;
            if((d|0)==(c[m>>2]|0))d=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else d=c[d>>2]|0;
            if(Xg(d,v,g,w,t,o,z,x,u,s)|0)
            {
                l=n;
                break
            }
            d=c[l>>2]|0;
            if((d|0)==(c[m>>2]|0))
            {
                Qb[c[(c[k>>2]|0)+40>>2]&63](k)|0;
                continue
            }
            else
            {
                c[l>>2]=d+4;
                continue
            }
        }
        s=a[z>>0]|0;
        d=c[u>>2]|0;
        if((((s&1)==0?(s&255)>>>1:c[z+4>>2]|0)|0)!=0?(d-x|0)<160:0)
        {
            s=c[t>>2]|0;
            t=d+4|0;
            c[u>>2]=t;
            c[d>>2]=s;
            d=t
        }
        b[j>>1]=Nn(g,c[w>>2]|0,h,v)|0;
        Gj(z,x,d,h);
        if(k)
        {
            d=c[k+12>>2]|0;
            if((d|0)==(c[k+16>>2]|0))d=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else d=c[d>>2]|0;
            if((d|0)==-1)
            {
                c[e>>2]=0;
                g=1
            }
            else g=0
        }
        else g=1;
        do if(l)
        {
            d=c[l+12>>2]|0;
            if((d|0)==(c[l+16>>2]|0))d=Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0;
            else d=c[d>>2]|0;
            if((d|0)!=-1)if(g)break;
            else
            {
                B=46;
                break
            }
            else
            {
                c[f>>2]=0;
                B=44;
                break
            }
        }
        else B=44;
        while(0);
        if((B|0)==44?g:0)B=46;
        if((B|0)==46)c[h>>2]=c[h>>2]|2;
        B=c[e>>2]|0;
        Ke(y);
        Ke(z);
        i=A;
        return B|0
    }
    function um(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;
        z=i;
        i=i+304|0;
        r=z+200|0;
        j=z;
        y=z+4|0;
        x=z+16|0;
        v=z+28|0;
        w=z+32|0;
        t=z+192|0;
        s=z+196|0;
        u=Rl(f)|0;
        $g(y,f,r,j);
        c[x>>2]=0;
        c[x+4>>2]=0;
        c[x+8>>2]=0;
        if(!(a[x>>0]&1))b=10;
        else b=(c[x>>2]&-2)+-1|0;
        Ne(x,b,0);
        o=x+8|0;
        p=x+1|0;
        f=(a[x>>0]&1)==0?p:c[o>>2]|0;
        c[v>>2]=f;
        c[t>>2]=w;
        c[s>>2]=0;
        q=x+4|0;
        n=c[j>>2]|0;
        j=c[d>>2]|0;
        a:while(1)
            {
            if(j)
            {
                b=c[j+12>>2]|0;
                if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
                else b=c[b>>2]|0;
                if((b|0)==-1)
                {
                    c[d>>2]=0;
                    j=0;
                    l=1
                }
                else l=0
            }
            else
            {
                j=0;
                l=1
            }
            k=c[e>>2]|0;
            do if(k)
            {
                b=c[k+12>>2]|0;
                if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else b=c[b>>2]|0;
                if((b|0)!=-1)if(l)
                {
                    m=k;
                    break
                }
                else break a;
                else
                {
                    c[e>>2]=0;
                    A=16;
                    break
                }
            }
            else A=16;
            while(0);
            if((A|0)==16)
            {
                A=0;
                if(l)
                {
                    k=0;
                    break
                }
                else m=0
            }
            k=a[x>>0]|0;
            k=(k&1)==0?(k&255)>>>1:c[q>>2]|0;
            if((c[v>>2]|0)==(f+k|0))
            {
                Ne(x,k<<1,0);
                if(!(a[x>>0]&1))b=10;
                else b=(c[x>>2]&-2)+-1|0;
                Ne(x,b,0);
                f=(a[x>>0]&1)==0?p:c[o>>2]|0;
                c[v>>2]=f+k
            }
            k=j+12|0;
            b=c[k>>2]|0;
            l=j+16|0;
            if((b|0)==(c[l>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if(Xg(b,u,f,v,s,n,y,w,t,r)|0)
            {
                k=m;
                break
            }
            b=c[k>>2]|0;
            if((b|0)==(c[l>>2]|0))
            {
                Qb[c[(c[j>>2]|0)+40>>2]&63](j)|0;
                continue
            }
            else
            {
                c[k>>2]=b+4;
                continue
            }
        }
        r=a[y>>0]|0;
        b=c[t>>2]|0;
        if((((r&1)==0?(r&255)>>>1:c[y+4>>2]|0)|0)!=0?(b-w|0)<160:0)
        {
            r=c[s>>2]|0;
            s=b+4|0;
            c[t>>2]=s;
            c[b>>2]=r;
            b=s
        }
        c[h>>2]=Mn(f,c[v>>2]|0,g,u)|0;
        Gj(y,w,b,g);
        if(j)
        {
            b=c[j+12>>2]|0;
            if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                f=1
            }
            else f=0
        }
        else f=1;
        do if(k)
        {
            b=c[k+12>>2]|0;
            if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(f)break;
            else
            {
                A=46;
                break
            }
            else
            {
                c[e>>2]=0;
                A=44;
                break
            }
        }
        else A=44;
        while(0);
        if((A|0)==44?f:0)A=46;
        if((A|0)==46)c[g>>2]=c[g>>2]|2;
        A=c[d>>2]|0;
        Ke(x);
        Ke(y);
        i=z;
        return A|0
    }
    function vm(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;
        z=i;
        i=i+304|0;
        r=z+200|0;
        j=z;
        y=z+4|0;
        x=z+16|0;
        v=z+28|0;
        w=z+32|0;
        t=z+192|0;
        s=z+196|0;
        u=Rl(f)|0;
        $g(y,f,r,j);
        c[x>>2]=0;
        c[x+4>>2]=0;
        c[x+8>>2]=0;
        if(!(a[x>>0]&1))b=10;
        else b=(c[x>>2]&-2)+-1|0;
        Ne(x,b,0);
        o=x+8|0;
        p=x+1|0;
        f=(a[x>>0]&1)==0?p:c[o>>2]|0;
        c[v>>2]=f;
        c[t>>2]=w;
        c[s>>2]=0;
        q=x+4|0;
        n=c[j>>2]|0;
        j=c[d>>2]|0;
        a:while(1)
            {
            if(j)
            {
                b=c[j+12>>2]|0;
                if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
                else b=c[b>>2]|0;
                if((b|0)==-1)
                {
                    c[d>>2]=0;
                    j=0;
                    l=1
                }
                else l=0
            }
            else
            {
                j=0;
                l=1
            }
            k=c[e>>2]|0;
            do if(k)
            {
                b=c[k+12>>2]|0;
                if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else b=c[b>>2]|0;
                if((b|0)!=-1)if(l)
                {
                    m=k;
                    break
                }
                else break a;
                else
                {
                    c[e>>2]=0;
                    A=16;
                    break
                }
            }
            else A=16;
            while(0);
            if((A|0)==16)
            {
                A=0;
                if(l)
                {
                    k=0;
                    break
                }
                else m=0
            }
            k=a[x>>0]|0;
            k=(k&1)==0?(k&255)>>>1:c[q>>2]|0;
            if((c[v>>2]|0)==(f+k|0))
            {
                Ne(x,k<<1,0);
                if(!(a[x>>0]&1))b=10;
                else b=(c[x>>2]&-2)+-1|0;
                Ne(x,b,0);
                f=(a[x>>0]&1)==0?p:c[o>>2]|0;
                c[v>>2]=f+k
            }
            k=j+12|0;
            b=c[k>>2]|0;
            l=j+16|0;
            if((b|0)==(c[l>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if(Xg(b,u,f,v,s,n,y,w,t,r)|0)
            {
                k=m;
                break
            }
            b=c[k>>2]|0;
            if((b|0)==(c[l>>2]|0))
            {
                Qb[c[(c[j>>2]|0)+40>>2]&63](j)|0;
                continue
            }
            else
            {
                c[k>>2]=b+4;
                continue
            }
        }
        r=a[y>>0]|0;
        b=c[t>>2]|0;
        if((((r&1)==0?(r&255)>>>1:c[y+4>>2]|0)|0)!=0?(b-w|0)<160:0)
        {
            r=c[s>>2]|0;
            s=b+4|0;
            c[t>>2]=s;
            c[b>>2]=r;
            b=s
        }
        c[h>>2]=Ln(f,c[v>>2]|0,g,u)|0;
        Gj(y,w,b,g);
        if(j)
        {
            b=c[j+12>>2]|0;
            if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                f=1
            }
            else f=0
        }
        else f=1;
        do if(k)
        {
            b=c[k+12>>2]|0;
            if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(f)break;
            else
            {
                A=46;
                break
            }
            else
            {
                c[e>>2]=0;
                A=44;
                break
            }
        }
        else A=44;
        while(0);
        if((A|0)==44?f:0)A=46;
        if((A|0)==46)c[g>>2]=c[g>>2]|2;
        A=c[d>>2]|0;
        Ke(x);
        Ke(y);
        i=z;
        return A|0
    }
    function wm(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;
        z=i;
        i=i+304|0;
        r=z+200|0;
        j=z;
        y=z+4|0;
        x=z+16|0;
        v=z+28|0;
        w=z+32|0;
        t=z+192|0;
        s=z+196|0;
        u=Rl(f)|0;
        $g(y,f,r,j);
        c[x>>2]=0;
        c[x+4>>2]=0;
        c[x+8>>2]=0;
        if(!(a[x>>0]&1))b=10;
        else b=(c[x>>2]&-2)+-1|0;
        Ne(x,b,0);
        o=x+8|0;
        p=x+1|0;
        f=(a[x>>0]&1)==0?p:c[o>>2]|0;
        c[v>>2]=f;
        c[t>>2]=w;
        c[s>>2]=0;
        q=x+4|0;
        n=c[j>>2]|0;
        j=c[d>>2]|0;
        a:while(1)
            {
            if(j)
            {
                b=c[j+12>>2]|0;
                if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
                else b=c[b>>2]|0;
                if((b|0)==-1)
                {
                    c[d>>2]=0;
                    j=0;
                    l=1
                }
                else l=0
            }
            else
            {
                j=0;
                l=1
            }
            k=c[e>>2]|0;
            do if(k)
            {
                b=c[k+12>>2]|0;
                if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else b=c[b>>2]|0;
                if((b|0)!=-1)if(l)
                {
                    m=k;
                    break
                }
                else break a;
                else
                {
                    c[e>>2]=0;
                    A=16;
                    break
                }
            }
            else A=16;
            while(0);
            if((A|0)==16)
            {
                A=0;
                if(l)
                {
                    k=0;
                    break
                }
                else m=0
            }
            k=a[x>>0]|0;
            k=(k&1)==0?(k&255)>>>1:c[q>>2]|0;
            if((c[v>>2]|0)==(f+k|0))
            {
                Ne(x,k<<1,0);
                if(!(a[x>>0]&1))b=10;
                else b=(c[x>>2]&-2)+-1|0;
                Ne(x,b,0);
                f=(a[x>>0]&1)==0?p:c[o>>2]|0;
                c[v>>2]=f+k
            }
            k=j+12|0;
            b=c[k>>2]|0;
            l=j+16|0;
            if((b|0)==(c[l>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if(Xg(b,u,f,v,s,n,y,w,t,r)|0)
            {
                k=m;
                break
            }
            b=c[k>>2]|0;
            if((b|0)==(c[l>>2]|0))
            {
                Qb[c[(c[j>>2]|0)+40>>2]&63](j)|0;
                continue
            }
            else
            {
                c[k>>2]=b+4;
                continue
            }
        }
        r=a[y>>0]|0;
        b=c[t>>2]|0;
        if((((r&1)==0?(r&255)>>>1:c[y+4>>2]|0)|0)!=0?(b-w|0)<160:0)
        {
            r=c[s>>2]|0;
            s=b+4|0;
            c[t>>2]=s;
            c[b>>2]=r;
            b=s
        }
        v=Kn(f,c[v>>2]|0,g,u)|0;
        c[h>>2]=v;
        c[h+4>>2]=G;
        Gj(y,w,b,g);
        if(j)
        {
            b=c[j+12>>2]|0;
            if((b|0)==(c[j+16>>2]|0))b=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else b=c[b>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                f=1
            }
            else f=0
        }
        else f=1;
        do if(k)
        {
            b=c[k+12>>2]|0;
            if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(f)break;
            else
            {
                A=46;
                break
            }
            else
            {
                c[e>>2]=0;
                A=44;
                break
            }
        }
        else A=44;
        while(0);
        if((A|0)==44?f:0)A=46;
        if((A|0)==46)c[g>>2]=c[g>>2]|2;
        A=c[d>>2]|0;
        Ke(x);
        Ke(y);
        i=z;
        return A|0
    }
    function xm(b,d,e,f,h,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        h=h|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0;
        C=i;
        i=i+352|0;
        t=C+176|0;
        k=C+332|0;
        l=C+328|0;
        B=C+316|0;
        A=C+304|0;
        y=C+168|0;
        z=C+8|0;
        w=C+4|0;
        v=C;
        x=C+337|0;
        u=C+336|0;
        ah(B,f,t,k,l);
        c[A>>2]=0;
        c[A+4>>2]=0;
        c[A+8>>2]=0;
        if(!(a[A>>0]&1))b=10;
        else b=(c[A>>2]&-2)+-1|0;
        Ne(A,b,0);
        q=A+8|0;
        r=A+1|0;
        f=(a[A>>0]&1)==0?r:c[q>>2]|0;
        c[y>>2]=f;
        c[w>>2]=z;
        c[v>>2]=0;
        a[x>>0]=1;
        a[u>>0]=69;
        s=A+4|0;
        p=c[k>>2]|0;
        o=c[l>>2]|0;
        k=c[d>>2]|0;
        a:while(1)
            {
            if(k)
            {
                b=c[k+12>>2]|0;
                if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else b=c[b>>2]|0;
                if((b|0)==-1)
                {
                    c[d>>2]=0;
                    k=0;
                    m=1
                }
                else m=0
            }
            else
            {
                k=0;
                m=1
            }
            l=c[e>>2]|0;
            do if(l)
            {
                b=c[l+12>>2]|0;
                if((b|0)==(c[l+16>>2]|0))b=Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0;
                else b=c[b>>2]|0;
                if((b|0)!=-1)if(m)break;
                else break a;
                else
                {
                    c[e>>2]=0;
                    D=16;
                    break
                }
            }
            else D=16;
            while(0);
            if((D|0)==16)
            {
                D=0;
                if(m)
                {
                    l=0;
                    break
                }
                else l=0
            }
            m=a[A>>0]|0;
            m=(m&1)==0?(m&255)>>>1:c[s>>2]|0;
            if((c[y>>2]|0)==(f+m|0))
            {
                Ne(A,m<<1,0);
                if(!(a[A>>0]&1))b=10;
                else b=(c[A>>2]&-2)+-1|0;
                Ne(A,b,0);
                f=(a[A>>0]&1)==0?r:c[q>>2]|0;
                c[y>>2]=f+m
            }
            m=k+12|0;
            b=c[m>>2]|0;
            n=k+16|0;
            if((b|0)==(c[n>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if(bh(b,x,u,f,y,p,o,B,z,w,v,t)|0)break;
            b=c[m>>2]|0;
            if((b|0)==(c[n>>2]|0))
            {
                Qb[c[(c[k>>2]|0)+40>>2]&63](k)|0;
                continue
            }
            else
            {
                c[m>>2]=b+4;
                continue
            }
        }
        u=a[B>>0]|0;
        b=c[w>>2]|0;
        if(!((a[x>>0]|0)==0?1:(((u&1)==0?(u&255)>>>1:c[B+4>>2]|0)|0)==0)?(b-z|0)<160:0)
        {
            v=c[v>>2]|0;
            x=b+4|0;
            c[w>>2]=x;
            c[b>>2]=v;
            b=x
        }
        g[j>>2]=+Jn(f,c[y>>2]|0,h);
        Gj(B,z,b,h);
        if(k)
        {
            b=c[k+12>>2]|0;
            if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                f=1
            }
            else f=0
        }
        else f=1;
        do if(l)
        {
            b=c[l+12>>2]|0;
            if((b|0)==(c[l+16>>2]|0))b=Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(f)break;
            else
            {
                D=46;
                break
            }
            else
            {
                c[e>>2]=0;
                D=44;
                break
            }
        }
        else D=44;
        while(0);
        if((D|0)==44?f:0)D=46;
        if((D|0)==46)c[h>>2]=c[h>>2]|2;
        D=c[d>>2]|0;
        Ke(A);
        Ke(B);
        i=C;
        return D|0
    }
    function ym(b,d,e,f,g,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0;
        C=i;
        i=i+352|0;
        t=C+176|0;
        k=C+332|0;
        l=C+328|0;
        B=C+316|0;
        A=C+304|0;
        y=C+168|0;
        z=C+8|0;
        w=C+4|0;
        v=C;
        x=C+337|0;
        u=C+336|0;
        ah(B,f,t,k,l);
        c[A>>2]=0;
        c[A+4>>2]=0;
        c[A+8>>2]=0;
        if(!(a[A>>0]&1))b=10;
        else b=(c[A>>2]&-2)+-1|0;
        Ne(A,b,0);
        q=A+8|0;
        r=A+1|0;
        f=(a[A>>0]&1)==0?r:c[q>>2]|0;
        c[y>>2]=f;
        c[w>>2]=z;
        c[v>>2]=0;
        a[x>>0]=1;
        a[u>>0]=69;
        s=A+4|0;
        p=c[k>>2]|0;
        o=c[l>>2]|0;
        k=c[d>>2]|0;
        a:while(1)
            {
            if(k)
            {
                b=c[k+12>>2]|0;
                if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else b=c[b>>2]|0;
                if((b|0)==-1)
                {
                    c[d>>2]=0;
                    k=0;
                    m=1
                }
                else m=0
            }
            else
            {
                k=0;
                m=1
            }
            l=c[e>>2]|0;
            do if(l)
            {
                b=c[l+12>>2]|0;
                if((b|0)==(c[l+16>>2]|0))b=Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0;
                else b=c[b>>2]|0;
                if((b|0)!=-1)if(m)break;
                else break a;
                else
                {
                    c[e>>2]=0;
                    D=16;
                    break
                }
            }
            else D=16;
            while(0);
            if((D|0)==16)
            {
                D=0;
                if(m)
                {
                    l=0;
                    break
                }
                else l=0
            }
            m=a[A>>0]|0;
            m=(m&1)==0?(m&255)>>>1:c[s>>2]|0;
            if((c[y>>2]|0)==(f+m|0))
            {
                Ne(A,m<<1,0);
                if(!(a[A>>0]&1))b=10;
                else b=(c[A>>2]&-2)+-1|0;
                Ne(A,b,0);
                f=(a[A>>0]&1)==0?r:c[q>>2]|0;
                c[y>>2]=f+m
            }
            m=k+12|0;
            b=c[m>>2]|0;
            n=k+16|0;
            if((b|0)==(c[n>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if(bh(b,x,u,f,y,p,o,B,z,w,v,t)|0)break;
            b=c[m>>2]|0;
            if((b|0)==(c[n>>2]|0))
            {
                Qb[c[(c[k>>2]|0)+40>>2]&63](k)|0;
                continue
            }
            else
            {
                c[m>>2]=b+4;
                continue
            }
        }
        u=a[B>>0]|0;
        b=c[w>>2]|0;
        if(!((a[x>>0]|0)==0?1:(((u&1)==0?(u&255)>>>1:c[B+4>>2]|0)|0)==0)?(b-z|0)<160:0)
        {
            v=c[v>>2]|0;
            x=b+4|0;
            c[w>>2]=x;
            c[b>>2]=v;
            b=x
        }
        h[j>>3]=+In(f,c[y>>2]|0,g);
        Gj(B,z,b,g);
        if(k)
        {
            b=c[k+12>>2]|0;
            if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                f=1
            }
            else f=0
        }
        else f=1;
        do if(l)
        {
            b=c[l+12>>2]|0;
            if((b|0)==(c[l+16>>2]|0))b=Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(f)break;
            else
            {
                D=46;
                break
            }
            else
            {
                c[e>>2]=0;
                D=44;
                break
            }
        }
        else D=44;
        while(0);
        if((D|0)==44?f:0)D=46;
        if((D|0)==46)c[g>>2]=c[g>>2]|2;
        D=c[d>>2]|0;
        Ke(A);
        Ke(B);
        i=C;
        return D|0
    }
    function zm(b,d,e,f,g,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0;
        C=i;
        i=i+352|0;
        t=C+176|0;
        k=C+332|0;
        l=C+328|0;
        B=C+316|0;
        A=C+304|0;
        y=C+168|0;
        z=C+8|0;
        w=C+4|0;
        v=C;
        x=C+337|0;
        u=C+336|0;
        ah(B,f,t,k,l);
        c[A>>2]=0;
        c[A+4>>2]=0;
        c[A+8>>2]=0;
        if(!(a[A>>0]&1))b=10;
        else b=(c[A>>2]&-2)+-1|0;
        Ne(A,b,0);
        q=A+8|0;
        r=A+1|0;
        f=(a[A>>0]&1)==0?r:c[q>>2]|0;
        c[y>>2]=f;
        c[w>>2]=z;
        c[v>>2]=0;
        a[x>>0]=1;
        a[u>>0]=69;
        s=A+4|0;
        p=c[k>>2]|0;
        o=c[l>>2]|0;
        k=c[d>>2]|0;
        a:while(1)
            {
            if(k)
            {
                b=c[k+12>>2]|0;
                if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
                else b=c[b>>2]|0;
                if((b|0)==-1)
                {
                    c[d>>2]=0;
                    k=0;
                    m=1
                }
                else m=0
            }
            else
            {
                k=0;
                m=1
            }
            l=c[e>>2]|0;
            do if(l)
            {
                b=c[l+12>>2]|0;
                if((b|0)==(c[l+16>>2]|0))b=Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0;
                else b=c[b>>2]|0;
                if((b|0)!=-1)if(m)break;
                else break a;
                else
                {
                    c[e>>2]=0;
                    D=16;
                    break
                }
            }
            else D=16;
            while(0);
            if((D|0)==16)
            {
                D=0;
                if(m)
                {
                    l=0;
                    break
                }
                else l=0
            }
            m=a[A>>0]|0;
            m=(m&1)==0?(m&255)>>>1:c[s>>2]|0;
            if((c[y>>2]|0)==(f+m|0))
            {
                Ne(A,m<<1,0);
                if(!(a[A>>0]&1))b=10;
                else b=(c[A>>2]&-2)+-1|0;
                Ne(A,b,0);
                f=(a[A>>0]&1)==0?r:c[q>>2]|0;
                c[y>>2]=f+m
            }
            m=k+12|0;
            b=c[m>>2]|0;
            n=k+16|0;
            if((b|0)==(c[n>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if(bh(b,x,u,f,y,p,o,B,z,w,v,t)|0)break;
            b=c[m>>2]|0;
            if((b|0)==(c[n>>2]|0))
            {
                Qb[c[(c[k>>2]|0)+40>>2]&63](k)|0;
                continue
            }
            else
            {
                c[m>>2]=b+4;
                continue
            }
        }
        u=a[B>>0]|0;
        b=c[w>>2]|0;
        if(!((a[x>>0]|0)==0?1:(((u&1)==0?(u&255)>>>1:c[B+4>>2]|0)|0)==0)?(b-z|0)<160:0)
        {
            v=c[v>>2]|0;
            x=b+4|0;
            c[w>>2]=x;
            c[b>>2]=v;
            b=x
        }
        h[j>>3]=+Hn(f,c[y>>2]|0,g);
        Gj(B,z,b,g);
        if(k)
        {
            b=c[k+12>>2]|0;
            if((b|0)==(c[k+16>>2]|0))b=Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0;
            else b=c[b>>2]|0;
            if((b|0)==-1)
            {
                c[d>>2]=0;
                f=1
            }
            else f=0
        }
        else f=1;
        do if(l)
        {
            b=c[l+12>>2]|0;
            if((b|0)==(c[l+16>>2]|0))b=Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0;
            else b=c[b>>2]|0;
            if((b|0)!=-1)if(f)break;
            else
            {
                D=46;
                break
            }
            else
            {
                c[e>>2]=0;
                D=44;
                break
            }
        }
        else D=44;
        while(0);
        if((D|0)==44?f:0)D=46;
        if((D|0)==46)c[g>>2]=c[g>>2]|2;
        D=c[d>>2]|0;
        Ke(A);
        Ke(B);
        i=C;
        return D|0
    }
    function Am(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0;
        g=i;
        i=i+16|0;
        h=g;
        c[h>>2]=f;
        f=Va(d|0)|0;
        d=Td(a,b,e,h)|0;
        if(f)Va(f|0)|0;
        i=g;
        return d|0
    }
    function Bm(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        f=i;
        i=i+16|0;
        g=f;
        c[g>>2]=e;
        e=Va(b|0)|0;
        b=zc(a,d,g)|0;
        if(e)Va(e|0)|0;
        i=f;
        return b|0
    }
    function Cm(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        o=i;
        i=i+16|0;
        n=o;
        j=c[b>>2]|0;
        a:do if(!j)j=0;
        else
        {
            p=d;
            l=f-p>>2;
            m=g+12|0;
            g=c[m>>2]|0;
            l=(g|0)>(l|0)?g-l|0:0;
            g=e;
            p=g-p|0;
            k=p>>2;
            if((p|0)>0?(Jb[c[(c[j>>2]|0)+48>>2]&31](j,d,k)|0)!=(k|0):0)
            {
                c[b>>2]=0;
                j=0;
                break
            }
            do if((l|0)>0)
            {
                Ue(n,l,h);
                if((Jb[c[(c[j>>2]|0)+48>>2]&31](j,(a[n>>0]&1)==0?n+4|0:c[n+8>>2]|0,l)|0)==(l|0))
                {
                    Ve(n);
                    break
                }
                else
                {
                    c[b>>2]=0;
                    Ve(n);
                    j=0;
                    break a
                }
            }
            while(0);
            p=f-g|0;
            f=p>>2;
            if((p|0)>0?(Jb[c[(c[j>>2]|0)+48>>2]&31](j,e,f)|0)!=(f|0):0)
            {
                c[b>>2]=0;
                j=0;
                break
            }
            c[m>>2]=0
        }
        while(0);
        i=o;
        return j|0
    }
    function Dm(a,e,f,g,h)
    {
        a=a|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;
        i=c[a>>2]|0;
        do if(i)
        {
            if((c[i+12>>2]|0)==(c[i+16>>2]|0))if((Qb[c[(c[i>>2]|0)+36>>2]&63](i)|0)==-1)
            {
                c[a>>2]=0;
                i=0;
                break
            }
            else
            {
                i=c[a>>2]|0;
                break
            }
        }
        else i=0;
        while(0);
        j=(i|0)==0;
        i=c[e>>2]|0;
        do if(i)
        {
            if((c[i+12>>2]|0)==(c[i+16>>2]|0)?(Qb[c[(c[i>>2]|0)+36>>2]&63](i)|0)==-1:0)
            {
                c[e>>2]=0;
                r=11;
                break
            }
            if(j)r=13;
            else r=12
        }
        else r=11;
        while(0);
        if((r|0)==11)if(j)r=12;
        else
        {
            i=0;
            r=13
        }
        a:do if((r|0)==12)
        {
            c[f>>2]=c[f>>2]|6;
            i=0
        }
        else if((r|0)==13)
        {
            j=c[a>>2]|0;
            k=c[j+12>>2]|0;
            if((k|0)==(c[j+16>>2]|0))j=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
            else j=d[k>>0]|0;
            k=j&255;
            if(k<<24>>24>-1?(q=g+8|0,(b[(c[q>>2]|0)+(j<<24>>24<<1)>>1]&2048)!=0):0)
            {
                m=(Jb[c[(c[g>>2]|0)+36>>2]&31](g,k,0)|0)<<24>>24;
                l=c[a>>2]|0;
                j=l+12|0;
                k=c[j>>2]|0;
                if((k|0)==(c[l+16>>2]|0))
                {
                    Qb[c[(c[l>>2]|0)+40>>2]&63](l)|0;
                    n=h;
                    h=i;
                    j=i;
                    i=m
                }
                else
                {
                    c[j>>2]=k+1;
                    n=h;
                    h=i;
                    j=i;
                    i=m
                }
                while(1)
                {
                    i=i+-48|0;
                    p=n+-1|0;
                    l=c[a>>2]|0;
                    do if(l)
                    {
                        if((c[l+12>>2]|0)==(c[l+16>>2]|0))if((Qb[c[(c[l>>2]|0)+36>>2]&63](l)|0)==-1)
                        {
                            c[a>>2]=0;
                            l=0;
                            break
                        }
                        else
                        {
                            l=c[a>>2]|0;
                            break
                        }
                    }
                    else l=0;
                    while(0);
                    l=(l|0)==0;
                    if(j)if((c[j+12>>2]|0)==(c[j+16>>2]|0))if((Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0)==-1)
                    {
                        c[e>>2]=0;
                        k=0;
                        o=0
                    }
                    else
                    {
                        k=h;
                        o=h
                    }
                    else
                    {
                        k=h;
                        o=j
                    }
                    else
                    {
                        k=h;
                        o=0
                    }
                    j=c[a>>2]|0;
                    if(!((n|0)>1&(l^(o|0)==0)))break;
                    l=c[j+12>>2]|0;
                    if((l|0)==(c[j+16>>2]|0))j=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
                    else j=d[l>>0]|0;
                    l=j&255;
                    if(l<<24>>24<=-1)break a;
                    if(!(b[(c[q>>2]|0)+(j<<24>>24<<1)>>1]&2048))break a;
                    i=((Jb[c[(c[g>>2]|0)+36>>2]&31](g,l,0)|0)<<24>>24)+(i*10|0)|0;
                    l=c[a>>2]|0;
                    j=l+12|0;
                    m=c[j>>2]|0;
                    if((m|0)==(c[l+16>>2]|0))
                    {
                        Qb[c[(c[l>>2]|0)+40>>2]&63](l)|0;
                        n=p;
                        h=k;
                        j=o;
                        continue
                    }
                    else
                    {
                        c[j>>2]=m+1;
                        n=p;
                        h=k;
                        j=o;
                        continue
                    }
                }
                do if(j)
                {
                    if((c[j+12>>2]|0)==(c[j+16>>2]|0))if((Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0)==-1)
                    {
                        c[a>>2]=0;
                        j=0;
                        break
                    }
                    else
                    {
                        j=c[a>>2]|0;
                        break
                    }
                }
                else j=0;
                while(0);
                j=(j|0)==0;
                do if(k)
                {
                    if((c[k+12>>2]|0)==(c[k+16>>2]|0)?(Qb[c[(c[k>>2]|0)+36>>2]&63](k)|0)==-1:0)
                    {
                        c[e>>2]=0;
                        r=50;
                        break
                    }
                    if(j)break a
                }
                else r=50;
                while(0);
                if((r|0)==50?!j:0)break;
                c[f>>2]=c[f>>2]|2;
                break
            }
            c[f>>2]=c[f>>2]|4;
            i=0
        }
        while(0);
        return i|0
    }
    function Em(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        g=c[a>>2]|0;
        do if(g)
        {
            h=c[g+12>>2]|0;
            if((h|0)==(c[g+16>>2]|0))g=Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0;
            else g=c[h>>2]|0;
            if((g|0)==-1)
            {
                c[a>>2]=0;
                i=1;
                break
            }
            else
            {
                i=(c[a>>2]|0)==0;
                break
            }
        }
        else i=1;
        while(0);
        h=c[b>>2]|0;
        do if(h)
        {
            g=c[h+12>>2]|0;
            if((g|0)==(c[h+16>>2]|0))g=Qb[c[(c[h>>2]|0)+36>>2]&63](h)|0;
            else g=c[g>>2]|0;
            if((g|0)!=-1)if(i)
            {
                p=17;
                break
            }
            else
            {
                p=16;
                break
            }
            else
            {
                c[b>>2]=0;
                p=14;
                break
            }
        }
        else p=14;
        while(0);
        if((p|0)==14)if(i)p=16;
        else
        {
            h=0;
            p=17
        }
        a:do if((p|0)==16)
        {
            c[d>>2]=c[d>>2]|6;
            g=0
        }
        else if((p|0)==17)
        {
            g=c[a>>2]|0;
            i=c[g+12>>2]|0;
            if((i|0)==(c[g+16>>2]|0))g=Qb[c[(c[g>>2]|0)+36>>2]&63](g)|0;
            else g=c[i>>2]|0;
            if(!(Jb[c[(c[e>>2]|0)+12>>2]&31](e,2048,g)|0))
            {
                c[d>>2]=c[d>>2]|4;
                g=0;
                break
            }
            g=(Jb[c[(c[e>>2]|0)+52>>2]&31](e,g,0)|0)<<24>>24;
            i=c[a>>2]|0;
            j=i+12|0;
            k=c[j>>2]|0;
            if((k|0)==(c[i+16>>2]|0))
            {
                Qb[c[(c[i>>2]|0)+40>>2]&63](i)|0;
                n=f;
                m=h;
                i=h
            }
            else
            {
                c[j>>2]=k+4;
                n=f;
                m=h;
                i=h
            }
            while(1)
            {
                g=g+-48|0;
                o=n+-1|0;
                j=c[a>>2]|0;
                do if(j)
                {
                    h=c[j+12>>2]|0;
                    if((h|0)==(c[j+16>>2]|0))h=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
                    else h=c[h>>2]|0;
                    if((h|0)==-1)
                    {
                        c[a>>2]=0;
                        k=1;
                        break
                    }
                    else
                    {
                        k=(c[a>>2]|0)==0;
                        break
                    }
                }
                else k=1;
                while(0);
                do if(i)
                {
                    j=c[i+12>>2]|0;
                    if((j|0)==(c[i+16>>2]|0))h=Qb[c[(c[i>>2]|0)+36>>2]&63](i)|0;
                    else h=c[j>>2]|0;
                    if((h|0)==-1)
                    {
                        c[b>>2]=0;
                        l=0;
                        f=0;
                        j=1;
                        break
                    }
                    else
                    {
                        l=m;
                        f=m;
                        j=(m|0)==0;
                        break
                    }
                }
                else
                {
                    l=m;
                    f=0;
                    j=1
                }
                while(0);
                i=c[a>>2]|0;
                if(!((n|0)>1&(k^j)))
                {
                    j=l;
                    break
                }
                j=c[i+12>>2]|0;
                if((j|0)==(c[i+16>>2]|0))h=Qb[c[(c[i>>2]|0)+36>>2]&63](i)|0;
                else h=c[j>>2]|0;
                if(!(Jb[c[(c[e>>2]|0)+12>>2]&31](e,2048,h)|0))break a;
                g=((Jb[c[(c[e>>2]|0)+52>>2]&31](e,h,0)|0)<<24>>24)+(g*10|0)|0;
                j=c[a>>2]|0;
                h=j+12|0;
                i=c[h>>2]|0;
                if((i|0)==(c[j+16>>2]|0))
                {
                    Qb[c[(c[j>>2]|0)+40>>2]&63](j)|0;
                    n=o;
                    m=l;
                    i=f;
                    continue
                }
                else
                {
                    c[h>>2]=i+4;
                    n=o;
                    m=l;
                    i=f;
                    continue
                }
            }
            do if(i)
            {
                h=c[i+12>>2]|0;
                if((h|0)==(c[i+16>>2]|0))h=Qb[c[(c[i>>2]|0)+36>>2]&63](i)|0;
                else h=c[h>>2]|0;
                if((h|0)==-1)
                {
                    c[a>>2]=0;
                    i=1;
                    break
                }
                else
                {
                    i=(c[a>>2]|0)==0;
                    break
                }
            }
            else i=1;
            while(0);
            do if(j)
            {
                h=c[j+12>>2]|0;
                if((h|0)==(c[j+16>>2]|0))h=Qb[c[(c[j>>2]|0)+36>>2]&63](j)|0;
                else h=c[h>>2]|0;
                if((h|0)!=-1)if(i)break a;
                else break;
                else
                {
                    c[b>>2]=0;
                    p=60;
                    break
                }
            }
            else p=60;
            while(0);
            if((p|0)==60?!i:0)break;
            c[d>>2]=c[d>>2]|2
        }
        while(0);
        return g|0
    }
    function Fm(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,i=0;
        h=a+4|0;
        f=(c[h>>2]|0)!=96;
        e=c[a>>2]|0;
        i=e;
        g=(c[d>>2]|0)-i|0;
        g=g>>>0<2147483647?g<<1:-1;
        i=(c[b>>2]|0)-i|0;
        e=be(f?e:0,g)|0;
        if(!e)sd();
        if(!f)
        {
            f=c[a>>2]|0;
            c[a>>2]=e;
            if(f)
            {
                Mb[c[h>>2]&127](f);
                e=c[a>>2]|0
            }
        }
        else c[a>>2]=e;
        c[h>>2]=107;
        c[b>>2]=e+i;
        c[d>>2]=(c[a>>2]|0)+g;
        return
    }
    function Gm(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,i=0;
        h=a+4|0;
        f=(c[h>>2]|0)!=96;
        e=c[a>>2]|0;
        i=e;
        g=(c[d>>2]|0)-i|0;
        g=g>>>0<2147483647?g<<1:-1;
        i=(c[b>>2]|0)-i>>2;
        e=be(f?e:0,g)|0;
        if(!e)sd();
        if(!f)
        {
            f=c[a>>2]|0;
            c[a>>2]=e;
            if(f)
            {
                Mb[c[h>>2]&127](f);
                e=c[a>>2]|0
            }
        }
        else c[a>>2]=e;
        c[h>>2]=107;
        c[b>>2]=e+(i<<2);
        c[d>>2]=(c[a>>2]|0)+(g>>>2<<2);
        return
    }
    function Hm(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,i=0,j=0,k=0;
        h=d;
        f=a[b>>0]|0;
        if(!(f&1))
        {
            g=10;
            k=(f&255)>>>1
        }
        else
        {
            f=c[b>>2]|0;
            g=(f&-2)+-1|0;
            k=c[b+4>>2]|0;
            f=f&255
        }
        j=e-h|0;
        do if((e|0)!=(d|0))
        {
            if((g-k|0)>>>0<j>>>0)
            {
                Se(b,g,k+j-g|0,k,k,0,0);
                f=a[b>>0]|0
            }
            if(!(f&1))i=b+1|0;
            else i=c[b+8>>2]|0;
            h=e+(k-h)|0;
            if((d|0)!=(e|0))
            {
                f=d;
                g=i+k|0;
                while(1)
                {
                    a[g>>0]=a[f>>0]|0;
                    f=f+1|0;
                    if((f|0)==(e|0))break;
                    else g=g+1|0
                }
            }
            a[i+h>>0]=0;
            f=k+j|0;
            if(!(a[b>>0]&1))
            {
                a[b>>0]=f<<1;
                break
            }
            else
            {
                c[b+4>>2]=f;
                break
            }
        }
        while(0);
        return b|0
    }
    function Im(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,i=0;
        h=a+4|0;
        f=(c[h>>2]|0)!=96;
        e=c[a>>2]|0;
        i=e;
        g=(c[d>>2]|0)-i|0;
        g=g>>>0<2147483647?g<<1:-1;
        i=(c[b>>2]|0)-i>>2;
        e=be(f?e:0,g)|0;
        if(!e)sd();
        if(!f)
        {
            f=c[a>>2]|0;
            c[a>>2]=e;
            if(f)
            {
                Mb[c[h>>2]&127](f);
                e=c[a>>2]|0
            }
        }
        else c[a>>2]=e;
        c[h>>2]=107;
        c[b>>2]=e+(i<<2);
        c[d>>2]=(c[a>>2]|0)+(g>>>2<<2);
        return
    }
    function Jm(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,i=0,j=0,k=0;
        h=d;
        f=a[b>>0]|0;
        if(!(f&1))
        {
            g=1;
            k=(f&255)>>>1
        }
        else
        {
            f=c[b>>2]|0;
            g=(f&-2)+-1|0;
            k=c[b+4>>2]|0;
            f=f&255
        }
        j=e-h>>2;
        do if(j)
        {
            if((g-k|0)>>>0<j>>>0)
            {
                $e(b,g,k+j-g|0,k,k,0,0);
                f=a[b>>0]|0
            }
            if(!(f&1))i=b+4|0;
            else i=c[b+8>>2]|0;
            h=k+((e-h|0)>>>2)|0;
            if((d|0)!=(e|0))
            {
                f=d;
                g=i+(k<<2)|0;
                while(1)
                {
                    c[g>>2]=c[f>>2];
                    f=f+4|0;
                    if((f|0)==(e|0))break;
                    else g=g+4|0
                }
            }
            c[i+(h<<2)>>2]=0;
            f=k+j|0;
            if(!(a[b>>0]&1))
            {
                a[b>>0]=f<<1;
                break
            }
            else
            {
                c[b+4>>2]=f;
                break
            }
        }
        while(0);
        return b|0
    }
    function Km(b,d)
    {
        b=b|0;
        d=d|0;
        c[b>>2]=0;
        c[b+4>>2]=0;
        c[b+8>>2]=0;
        a[b+128>>0]=0;
        if(d)
        {
            Gn(b,d);
            Cn(b,d)
        }
        return
    }
    function Lm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(5432)|0);
        return
    }
    function Mm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(5472)|0);
        return
    }
    function Nm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6584)|0);
        return
    }
    function Om(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6576)|0);
        return
    }
    function Pm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6648)|0);
        return
    }
    function Qm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6656)|0);
        return
    }
    function Rm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6712)|0);
        return
    }
    function Sm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6720)|0);
        return
    }
    function Tm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6728)|0);
        return
    }
    function Um(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6736)|0);
        return
    }
    function Vm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(5544)|0);
        return
    }
    function Wm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(5656)|0);
        return
    }
    function Xm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(5720)|0);
        return
    }
    function Ym(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(5784)|0);
        return
    }
    function Zm(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6104)|0);
        return
    }
    function _m(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6168)|0);
        return
    }
    function $m(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6232)|0);
        return
    }
    function an(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6296)|0);
        return
    }
    function bn(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6336)|0);
        return
    }
    function cn(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6376)|0);
        return
    }
    function dn(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6416)|0);
        return
    }
    function en(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6456)|0);
        return
    }
    function fn(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(5880)|0);
        return
    }
    function gn(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(5976)|0);
        return
    }
    function hn(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6008)|0);
        return
    }
    function jn(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6040)|0);
        return
    }
    function kn(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6496)|0);
        return
    }
    function ln(a,b)
    {
        a=a|0;
        b=b|0;
        jk(a,b,sk(6536)|0);
        return
    }
    function mn(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0,f=0,g=0,h=0;
        h=a+4|0;
        d=c[h>>2]|0;
        e=c[a>>2]|0;
        f=d-e>>2;
        if(f>>>0>=b>>>0)
        {
            if(f>>>0>b>>>0?(g=e+(b<<2)|0,(d|0)!=(g|0)):0)
            {
                do d=d+-4|0;
                while((d|0)!=(g|0));
                c[h>>2]=d
            }
        }
        else Bn(a,b-f|0);
        return
    }
    function nn(b)
    {
        b=b|0;
        var d=0,e=0,f=0;
        e=c[b>>2]|0;
        do if(e)
        {
            f=b+4|0;
            d=c[f>>2]|0;
            if((d|0)!=(e|0))
            {
                do d=d+-4|0;
                while((d|0)!=(e|0));
                c[f>>2]=d
            }
            if((b+16|0)==(e|0))
            {
                a[b+128>>0]=0;
                break
            }
            else
            {
                Lc(e);
                break
            }
        }
        while(0);
        return
    }
    function on(a)
    {
        a=a|0;
        var b=0,d=0;
        d=a+4|0;
        b=c[d>>2]|0;
        d=c[d+4>>2]|0;
        a=(c[a>>2]|0)+(d>>1)|0;
        if(d&1)b=c[(c[a>>2]|0)+b>>2]|0;
        Mb[b&127](a);
        return
    }
    function pn(d,f,g,h,i,j,k,l)
    {
        d=d|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        k=k|0;
        l=l|0;
        var m=0,n=0;
        c[g>>2]=d;
        c[j>>2]=h;
        if(l&2)if((i-h|0)<3)d=1;
        else
        {
            c[j>>2]=h+1;
            a[h>>0]=-17;
            m=c[j>>2]|0;
            c[j>>2]=m+1;
            a[m>>0]=-69;
            m=c[j>>2]|0;
            c[j>>2]=m+1;
            a[m>>0]=-65;
            m=4
        }
        else m=4;
        a:do if((m|0)==4)
        {
            n=f;
            d=c[g>>2]|0;
            if(d>>>0<f>>>0)while(1)
                {
                h=b[d>>1]|0;
                m=h&65535;
                if(m>>>0>k>>>0)
                {
                    d=2;
                    break a
                }
                do if((h&65535)<128)
                {
                    d=c[j>>2]|0;
                    if((i-d|0)<1)
                    {
                        d=1;
                        break a
                    }
                    c[j>>2]=d+1;
                    a[d>>0]=h
                }
                else
                {
                    if((h&65535)<2048)
                    {
                        d=c[j>>2]|0;
                        if((i-d|0)<2)
                        {
                            d=1;
                            break a
                        }
                        c[j>>2]=d+1;
                        a[d>>0]=m>>>6|192;
                        h=c[j>>2]|0;
                        c[j>>2]=h+1;
                        a[h>>0]=m&63|128;
                        break
                    }
                    if((h&65535)<55296)
                    {
                        d=c[j>>2]|0;
                        if((i-d|0)<3)
                        {
                            d=1;
                            break a
                        }
                        c[j>>2]=d+1;
                        a[d>>0]=m>>>12|224;
                        h=c[j>>2]|0;
                        c[j>>2]=h+1;
                        a[h>>0]=m>>>6&63|128;
                        h=c[j>>2]|0;
                        c[j>>2]=h+1;
                        a[h>>0]=m&63|128;
                        break
                    }
                    if((h&65535)>=56320)
                    {
                        if((h&65535)<57344)
                        {
                            d=2;
                            break a
                        }
                        d=c[j>>2]|0;
                        if((i-d|0)<3)
                        {
                            d=1;
                            break a
                        }
                        c[j>>2]=d+1;
                        a[d>>0]=m>>>12|224;
                        h=c[j>>2]|0;
                        c[j>>2]=h+1;
                        a[h>>0]=m>>>6&63|128;
                        h=c[j>>2]|0;
                        c[j>>2]=h+1;
                        a[h>>0]=m&63|128;
                        break
                    }
                    if((n-d|0)<4)
                    {
                        d=1;
                        break a
                    }
                    d=d+2|0;
                    h=e[d>>1]|0;
                    if((h&64512|0)!=56320)
                    {
                        d=2;
                        break a
                    }
                    if((i-(c[j>>2]|0)|0)<4)
                    {
                        d=1;
                        break a
                    }
                    l=m&960;
                    if(((l<<10)+65536|m<<10&64512|h&1023)>>>0>k>>>0)
                    {
                        d=2;
                        break a
                    }
                    c[g>>2]=d;
                    d=(l>>>6)+1|0;
                    l=c[j>>2]|0;
                    c[j>>2]=l+1;
                    a[l>>0]=d>>>2|240;
                    l=c[j>>2]|0;
                    c[j>>2]=l+1;
                    a[l>>0]=m>>>2&15|d<<4&48|128;
                    l=c[j>>2]|0;
                    c[j>>2]=l+1;
                    a[l>>0]=m<<4&48|h>>>6&15|128;
                    m=c[j>>2]|0;
                    c[j>>2]=m+1;
                    a[m>>0]=h&63|128
                }
                while(0);
                d=(c[g>>2]|0)+2|0;
                c[g>>2]=d;
                if(d>>>0>=f>>>0)
                {
                    d=0;
                    break
                }
            }
            else d=0
        }
        while(0);
        return d|0
    }
    function qn(e,f,g,h,i,j,k,l)
    {
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        k=k|0;
        l=l|0;
        var m=0,n=0,o=0,p=0,q=0,r=0;
        c[g>>2]=e;
        c[j>>2]=h;
        if(l&4)
        {
            e=c[g>>2]|0;
            l=f;
            if((((l-e|0)>2?(a[e>>0]|0)==-17:0)?(a[e+1>>0]|0)==-69:0)?(a[e+2>>0]|0)==-65:0)
            {
                c[g>>2]=e+3;
                m=c[j>>2]|0
            }
            else m=h
        }
        else
        {
            m=h;
            l=f
        }
        q=i;
        h=c[g>>2]|0;
        e=h>>>0<f>>>0;
        a:do if(e&m>>>0<i>>>0)while(1)
            {
            e=a[h>>0]|0;
            o=e&255;
            if(o>>>0>k>>>0)
            {
                e=2;
                break a
            }
            do if(e<<24>>24>-1)
            {
                b[m>>1]=e&255;
                c[g>>2]=h+1
            }
            else
            {
                if((e&255)<194)
                {
                    e=2;
                    break a
                }
                if((e&255)<224)
                {
                    if((l-h|0)<2)
                    {
                        e=1;
                        break a
                    }
                    e=d[h+1>>0]|0;
                    if((e&192|0)!=128)
                    {
                        e=2;
                        break a
                    }
                    e=e&63|o<<6&1984;
                    if(e>>>0>k>>>0)
                    {
                        e=2;
                        break a
                    }
                    b[m>>1]=e;
                    c[g>>2]=h+2;
                    break
                }
                if((e&255)<240)
                {
                    if((l-h|0)<3)
                    {
                        e=1;
                        break a
                    }
                    n=a[h+1>>0]|0;
                    e=a[h+2>>0]|0;
                    if((o|0)==224)
                    {
                        if((n&-32)<<24>>24!=-96)
                        {
                            e=2;
                            break a
                        }
                    }
                    else if((o|0)==237)
                    {
                        if((n&-32)<<24>>24!=-128)
                        {
                            e=2;
                            break a
                        }
                    }
                    else if((n&-64)<<24>>24!=-128)
                    {
                        e=2;
                        break a
                    }
                    e=e&255;
                    if((e&192|0)!=128)
                    {
                        e=2;
                        break a
                    }
                    e=(n&255)<<6&4032|o<<12|e&63;
                    if((e&65535)>>>0>k>>>0)
                    {
                        e=2;
                        break a
                    }
                    b[m>>1]=e;
                    c[g>>2]=h+3;
                    break
                }
                if((e&255)>=245)
                {
                    e=2;
                    break a
                }
                if((l-h|0)<4)
                {
                    e=1;
                    break a
                }
                n=a[h+1>>0]|0;
                e=a[h+2>>0]|0;
                h=a[h+3>>0]|0;
                if((o|0)==244)
                {
                    if((n&-16)<<24>>24!=-128)
                    {
                        e=2;
                        break a
                    }
                }
                else if((o|0)==240)
                {
                    if((n+112&255)>=48)
                    {
                        e=2;
                        break a
                    }
                }
                else if((n&-64)<<24>>24!=-128)
                {
                    e=2;
                    break a
                }
                p=e&255;
                if((p&192|0)!=128)
                {
                    e=2;
                    break a
                }
                e=h&255;
                if((e&192|0)!=128)
                {
                    e=2;
                    break a
                }
                if((q-m|0)<4)
                {
                    e=1;
                    break a
                }
                o=o&7;
                h=n&255;
                n=p<<6;
                e=e&63;
                if((h<<12&258048|o<<18|n&4032|e)>>>0>k>>>0)
                {
                    e=2;
                    break a
                }
                b[m>>1]=h<<2&60|p>>>4&3|((h>>>4&3|o<<2)<<6)+16320|55296;
                p=m+2|0;
                c[j>>2]=p;
                b[p>>1]=e|n&960|56320;
                c[g>>2]=(c[g>>2]|0)+4
            }
            while(0);
            m=(c[j>>2]|0)+2|0;
            c[j>>2]=m;
            h=c[g>>2]|0;
            e=h>>>0<f>>>0;
            if(!(e&m>>>0<i>>>0))
            {
                r=39;
                break
            }
        }
        else r=39;
        while(0);
        if((r|0)==39)e=e&1;
        return e|0
    }
    function rn(b,c,e,f,g)
    {
        b=b|0;
        c=c|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,i=0,j=0,k=0,l=0,m=0,n=0;
        n=c;
        if((((g&4|0)!=0?(n-b|0)>2:0)?(a[b>>0]|0)==-17:0)?(a[b+1>>0]|0)==-69:0)g=(a[b+2>>0]|0)==-65?b+3|0:b;
        else g=b;
        a:do if((e|0)!=0&g>>>0<c>>>0)
        {
            m=g;
            h=0;
            b:while(1)
                {
                g=a[m>>0]|0;
                l=g&255;
                if(l>>>0>f>>>0)
                {
                    g=m;
                    h=42;
                    break a
                }
                do if(g<<24>>24>-1)g=m+1|0;
                else
                {
                    if((g&255)<194)
                    {
                        g=m;
                        h=42;
                        break a
                    }
                    if((g&255)<224)
                    {
                        if((n-m|0)<2)
                        {
                            g=m;
                            h=42;
                            break a
                        }
                        g=d[m+1>>0]|0;
                        if((g&192|0)!=128)
                        {
                            g=m;
                            h=42;
                            break a
                        }
                        if((g&63|l<<6&1984)>>>0>f>>>0)
                        {
                            g=m;
                            h=42;
                            break a
                        }
                        g=m+2|0;
                        break
                    }
                    if((g&255)<240)
                    {
                        g=m;
                        if((n-g|0)<3)
                        {
                            g=m;
                            h=42;
                            break a
                        }
                        j=a[m+1>>0]|0;
                        i=a[m+2>>0]|0;
                        if((l|0)==224)
                        {
                            if((j&-32)<<24>>24!=-96)
                            {
                                h=20;
                                break b
                            }
                        }
                        else if((l|0)==237)
                        {
                            if((j&-32)<<24>>24!=-128)
                            {
                                h=22;
                                break b
                            }
                        }
                        else if((j&-64)<<24>>24!=-128)
                        {
                            h=24;
                            break b
                        }
                        g=i&255;
                        if((g&192|0)!=128)
                        {
                            g=m;
                            h=42;
                            break a
                        }
                        if(((j&255)<<6&4032|l<<12&61440|g&63)>>>0>f>>>0)
                        {
                            g=m;
                            h=42;
                            break a
                        }
                        g=m+3|0;
                        break
                    }
                    if((g&255)>=245)
                    {
                        g=m;
                        h=42;
                        break a
                    }
                    g=m;
                    if((e-h|0)>>>0<2|(n-g|0)<4)
                    {
                        g=m;
                        h=42;
                        break a
                    }
                    k=a[m+1>>0]|0;
                    j=a[m+2>>0]|0;
                    i=a[m+3>>0]|0;
                    if((l|0)==244)
                    {
                        if((k&-16)<<24>>24!=-128)
                        {
                            h=34;
                            break b
                        }
                    }
                    else if((l|0)==240)
                    {
                        if((k+112&255)>=48)
                        {
                            h=32;
                            break b
                        }
                    }
                    else if((k&-64)<<24>>24!=-128)
                    {
                        h=36;
                        break b
                    }
                    j=j&255;
                    if((j&192|0)!=128)
                    {
                        g=m;
                        h=42;
                        break a
                    }
                    g=i&255;
                    if((g&192|0)!=128)
                    {
                        g=m;
                        h=42;
                        break a
                    }
                    if(((k&255)<<12&258048|l<<18&1835008|j<<6&4032|g&63)>>>0>f>>>0)
                    {
                        g=m;
                        h=42;
                        break a
                    }
                    g=m+4|0;
                    h=h+1|0
                }
                while(0);
                h=h+1|0;
                if(!(h>>>0<e>>>0&g>>>0<c>>>0))
                {
                    h=42;
                    break a
                }
                else m=g
            }
            if((h|0)==20)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==22)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==24)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==32)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==34)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==36)
            {
                g=g-b|0;
                break
            }
        }
        else h=42;
        while(0);
        if((h|0)==42)g=g-b|0;
        return g|0
    }
    function sn(b,d,e,f,g,h,i,j)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        var k=0,l=0;
        c[e>>2]=b;
        c[h>>2]=f;
        l=g;
        if(j&2)if((l-f|0)<3)b=1;
        else
        {
            c[h>>2]=f+1;
            a[f>>0]=-17;
            k=c[h>>2]|0;
            c[h>>2]=k+1;
            a[k>>0]=-69;
            k=c[h>>2]|0;
            c[h>>2]=k+1;
            a[k>>0]=-65;
            k=4
        }
        else k=4;
        a:do if((k|0)==4)
        {
            b=c[e>>2]|0;
            if(b>>>0<d>>>0)while(1)
                {
                j=c[b>>2]|0;
                if(j>>>0>i>>>0|(j&-2048|0)==55296)
                {
                    b=2;
                    break a
                }
                do if(j>>>0>=128)
                {
                    if(j>>>0<2048)
                    {
                        b=c[h>>2]|0;
                        if((l-b|0)<2)
                        {
                            b=1;
                            break a
                        }
                        c[h>>2]=b+1;
                        a[b>>0]=j>>>6|192;
                        k=c[h>>2]|0;
                        c[h>>2]=k+1;
                        a[k>>0]=j&63|128;
                        break
                    }
                    b=c[h>>2]|0;
                    g=l-b|0;
                    if(j>>>0<65536)
                    {
                        if((g|0)<3)
                        {
                            b=1;
                            break a
                        }
                        c[h>>2]=b+1;
                        a[b>>0]=j>>>12|224;
                        k=c[h>>2]|0;
                        c[h>>2]=k+1;
                        a[k>>0]=j>>>6&63|128;
                        k=c[h>>2]|0;
                        c[h>>2]=k+1;
                        a[k>>0]=j&63|128;
                        break
                    }
                    else
                    {
                        if((g|0)<4)
                        {
                            b=1;
                            break a
                        }
                        c[h>>2]=b+1;
                        a[b>>0]=j>>>18|240;
                        k=c[h>>2]|0;
                        c[h>>2]=k+1;
                        a[k>>0]=j>>>12&63|128;
                        k=c[h>>2]|0;
                        c[h>>2]=k+1;
                        a[k>>0]=j>>>6&63|128;
                        k=c[h>>2]|0;
                        c[h>>2]=k+1;
                        a[k>>0]=j&63|128;
                        break
                    }
                }
                else
                {
                    b=c[h>>2]|0;
                    if((l-b|0)<1)
                    {
                        b=1;
                        break a
                    }
                    c[h>>2]=b+1;
                    a[b>>0]=j
                }
                while(0);
                b=(c[e>>2]|0)+4|0;
                c[e>>2]=b;
                if(b>>>0>=d>>>0)
                {
                    b=0;
                    break
                }
            }
            else b=0
        }
        while(0);
        return b|0
    }
    function tn(b,e,f,g,h,i,j,k)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        j=j|0;
        k=k|0;
        var l=0,m=0,n=0,o=0,p=0,q=0;
        c[f>>2]=b;
        c[i>>2]=g;
        if(k&4)
        {
            b=c[f>>2]|0;
            k=e;
            if((((k-b|0)>2?(a[b>>0]|0)==-17:0)?(a[b+1>>0]|0)==-69:0)?(a[b+2>>0]|0)==-65:0)
            {
                c[f>>2]=b+3;
                g=c[i>>2]|0;
                p=k
            }
            else p=k
        }
        else p=e;
        k=c[f>>2]|0;
        b=k>>>0<e>>>0;
        a:do if(b&g>>>0<h>>>0)while(1)
            {
            b=a[k>>0]|0;
            o=b&255;
            do if(b<<24>>24>-1)
            {
                if(o>>>0>j>>>0)
                {
                    b=2;
                    break a
                }
                c[g>>2]=o;
                c[f>>2]=k+1
            }
            else
            {
                if((b&255)<194)
                {
                    b=2;
                    break a
                }
                if((b&255)<224)
                {
                    if((p-k|0)<2)
                    {
                        b=1;
                        break a
                    }
                    b=d[k+1>>0]|0;
                    if((b&192|0)!=128)
                    {
                        b=2;
                        break a
                    }
                    b=b&63|o<<6&1984;
                    if(b>>>0>j>>>0)
                    {
                        b=2;
                        break a
                    }
                    c[g>>2]=b;
                    c[f>>2]=k+2;
                    break
                }
                if((b&255)<240)
                {
                    if((p-k|0)<3)
                    {
                        b=1;
                        break a
                    }
                    l=a[k+1>>0]|0;
                    b=a[k+2>>0]|0;
                    if((o|0)==237)
                    {
                        if((l&-32)<<24>>24!=-128)
                        {
                            b=2;
                            break a
                        }
                    }
                    else if((o|0)==224)
                    {
                        if((l&-32)<<24>>24!=-96)
                        {
                            b=2;
                            break a
                        }
                    }
                    else if((l&-64)<<24>>24!=-128)
                    {
                        b=2;
                        break a
                    }
                    b=b&255;
                    if((b&192|0)!=128)
                    {
                        b=2;
                        break a
                    }
                    b=(l&255)<<6&4032|o<<12&61440|b&63;
                    if(b>>>0>j>>>0)
                    {
                        b=2;
                        break a
                    }
                    c[g>>2]=b;
                    c[f>>2]=k+3;
                    break
                }
                if((b&255)>=245)
                {
                    b=2;
                    break a
                }
                if((p-k|0)<4)
                {
                    b=1;
                    break a
                }
                n=a[k+1>>0]|0;
                b=a[k+2>>0]|0;
                l=a[k+3>>0]|0;
                if((o|0)==244)
                {
                    if((n&-16)<<24>>24!=-128)
                    {
                        b=2;
                        break a
                    }
                }
                else if((o|0)==240)
                {
                    if((n+112&255)>=48)
                    {
                        b=2;
                        break a
                    }
                }
                else if((n&-64)<<24>>24!=-128)
                {
                    b=2;
                    break a
                }
                m=b&255;
                if((m&192|0)!=128)
                {
                    b=2;
                    break a
                }
                b=l&255;
                if((b&192|0)!=128)
                {
                    b=2;
                    break a
                }
                b=(n&255)<<12&258048|o<<18&1835008|m<<6&4032|b&63;
                if(b>>>0>j>>>0)
                {
                    b=2;
                    break a
                }
                c[g>>2]=b;
                c[f>>2]=k+4
            }
            while(0);
            g=(c[i>>2]|0)+4|0;
            c[i>>2]=g;
            k=c[f>>2]|0;
            b=k>>>0<e>>>0;
            if(!(b&g>>>0<h>>>0))
            {
                q=38;
                break
            }
        }
        else q=38;
        while(0);
        if((q|0)==38)b=b&1;
        return b|0
    }
    function un(b,c,e,f,g)
    {
        b=b|0;
        c=c|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,i=0,j=0,k=0,l=0,m=0,n=0;
        n=c;
        if((((g&4|0)!=0?(n-b|0)>2:0)?(a[b>>0]|0)==-17:0)?(a[b+1>>0]|0)==-69:0)g=(a[b+2>>0]|0)==-65?b+3|0:b;
        else g=b;
        a:do if((e|0)!=0&g>>>0<c>>>0)
        {
            l=g;
            m=0;
            b:while(1)
                {
                g=a[l>>0]|0;
                k=g&255;
                do if(g<<24>>24>-1)
                {
                    if(k>>>0>f>>>0)
                    {
                        g=l;
                        h=42;
                        break a
                    }
                    g=l+1|0
                }
                else
                {
                    if((g&255)<194)
                    {
                        g=l;
                        h=42;
                        break a
                    }
                    if((g&255)<224)
                    {
                        if((n-l|0)<2)
                        {
                            g=l;
                            h=42;
                            break a
                        }
                        g=d[l+1>>0]|0;
                        if((g&192|0)!=128)
                        {
                            g=l;
                            h=42;
                            break a
                        }
                        if((g&63|k<<6&1984)>>>0>f>>>0)
                        {
                            g=l;
                            h=42;
                            break a
                        }
                        g=l+2|0;
                        break
                    }
                    if((g&255)<240)
                    {
                        g=l;
                        if((n-g|0)<3)
                        {
                            g=l;
                            h=42;
                            break a
                        }
                        i=a[l+1>>0]|0;
                        h=a[l+2>>0]|0;
                        if((k|0)==224)
                        {
                            if((i&-32)<<24>>24!=-96)
                            {
                                h=20;
                                break b
                            }
                        }
                        else if((k|0)==237)
                        {
                            if((i&-32)<<24>>24!=-128)
                            {
                                h=22;
                                break b
                            }
                        }
                        else if((i&-64)<<24>>24!=-128)
                        {
                            h=24;
                            break b
                        }
                        g=h&255;
                        if((g&192|0)!=128)
                        {
                            g=l;
                            h=42;
                            break a
                        }
                        if(((i&255)<<6&4032|k<<12&61440|g&63)>>>0>f>>>0)
                        {
                            g=l;
                            h=42;
                            break a
                        }
                        g=l+3|0;
                        break
                    }
                    if((g&255)>=245)
                    {
                        g=l;
                        h=42;
                        break a
                    }
                    g=l;
                    if((n-g|0)<4)
                    {
                        g=l;
                        h=42;
                        break a
                    }
                    j=a[l+1>>0]|0;
                    h=a[l+2>>0]|0;
                    i=a[l+3>>0]|0;
                    if((k|0)==244)
                    {
                        if((j&-16)<<24>>24!=-128)
                        {
                            h=34;
                            break b
                        }
                    }
                    else if((k|0)==240)
                    {
                        if((j+112&255)>=48)
                        {
                            h=32;
                            break b
                        }
                    }
                    else if((j&-64)<<24>>24!=-128)
                    {
                        h=36;
                        break b
                    }
                    h=h&255;
                    if((h&192|0)!=128)
                    {
                        g=l;
                        h=42;
                        break a
                    }
                    g=i&255;
                    if((g&192|0)!=128)
                    {
                        g=l;
                        h=42;
                        break a
                    }
                    if(((j&255)<<12&258048|k<<18&1835008|h<<6&4032|g&63)>>>0>f>>>0)
                    {
                        g=l;
                        h=42;
                        break a
                    }
                    g=l+4|0
                }
                while(0);
                m=m+1|0;
                if(!(m>>>0<e>>>0&g>>>0<c>>>0))
                {
                    h=42;
                    break a
                }
                else l=g
            }
            if((h|0)==20)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==22)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==24)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==32)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==34)
            {
                g=g-b|0;
                break
            }
            else if((h|0)==36)
            {
                g=g-b|0;
                break
            }
        }
        else h=42;
        while(0);
        if((h|0)==42)g=g-b|0;
        return g|0
    }
    function vn(a)
    {
        a=a|0;
        Ke(10684);
        Ke(10672);
        Ke(10660);
        Ke(10648);
        Ke(10636);
        Ke(10624);
        Ke(10612);
        Ke(10600);
        Ke(10588);
        Ke(10576);
        Ke(10564);
        Ke(10552);
        Ke(10540);
        Ke(10528);
        return
    }
    function wn(a)
    {
        a=a|0;
        Ve(11012);
        Ve(11e3);
        Ve(10988);
        Ve(10976);
        Ve(10964);
        Ve(10952);
        Ve(10940);
        Ve(10928);
        Ve(10916);
        Ve(10904);
        Ve(10892);
        Ve(10880);
        Ve(10868);
        Ve(10856);
        return
    }
    function xn(a)
    {
        a=a|0;
        Ke(11684);
        Ke(11672);
        Ke(11660);
        Ke(11648);
        Ke(11636);
        Ke(11624);
        Ke(11612);
        Ke(11600);
        Ke(11588);
        Ke(11576);
        Ke(11564);
        Ke(11552);
        Ke(11540);
        Ke(11528);
        Ke(11516);
        Ke(11504);
        Ke(11492);
        Ke(11480);
        Ke(11468);
        Ke(11456);
        Ke(11444);
        Ke(11432);
        Ke(11420);
        Ke(11408);
        return
    }
    function yn(a)
    {
        a=a|0;
        Ve(12212);
        Ve(12200);
        Ve(12188);
        Ve(12176);
        Ve(12164);
        Ve(12152);
        Ve(12140);
        Ve(12128);
        Ve(12116);
        Ve(12104);
        Ve(12092);
        Ve(12080);
        Ve(12068);
        Ve(12056);
        Ve(12044);
        Ve(12032);
        Ve(12020);
        Ve(12008);
        Ve(11996);
        Ve(11984);
        Ve(11972);
        Ve(11960);
        Ve(11948);
        Ve(11936);
        return
    }
    function zn(a)
    {
        a=a|0;
        Ke(13068);
        Ke(13056);
        Ke(13044);
        Ke(13032);
        Ke(13020);
        Ke(13008);
        Ke(12996);
        Ke(12984);
        Ke(12972);
        Ke(12960);
        Ke(12948);
        Ke(12936);
        Ke(12924);
        Ke(12912);
        Ke(12900);
        Ke(12888);
        Ke(12876);
        Ke(12864);
        Ke(12852);
        Ke(12840);
        Ke(12828);
        Ke(12816);
        Ke(12804);
        Ke(12792);
        return
    }
    function An(a)
    {
        a=a|0;
        Ve(13396);
        Ve(13384);
        Ve(13372);
        Ve(13360);
        Ve(13348);
        Ve(13336);
        Ve(13324);
        Ve(13312);
        Ve(13300);
        Ve(13288);
        Ve(13276);
        Ve(13264);
        Ve(13252);
        Ve(13240);
        Ve(13228);
        Ve(13216);
        Ve(13204);
        Ve(13192);
        Ve(13180);
        Ve(13168);
        Ve(13156);
        Ve(13144);
        Ve(13132);
        Ve(13120);
        return
    }
    function Bn(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0,f=0,g=0,h=0,j=0,k=0;
        k=i;
        i=i+32|0;
        j=k;
        g=c[a+8>>2]|0;
        d=c[a+4>>2]|0;
        if(g-d>>2>>>0<b>>>0)
        {
            e=c[a>>2]|0;
            h=d-e>>2;
            f=h+b|0;
            if(f>>>0>1073741823)Ic(a);
            d=g-e|0;
            if(d>>2>>>0<536870911)
            {
                d=d>>1;
                d=d>>>0<f>>>0?f:d
            }
            else d=1073741823;
            Dn(j,d,h,a+16|0);
            h=j+8|0;
            f=c[h>>2]|0;
            Xn(f|0,0,b<<2|0)|0;
            c[h>>2]=f+(b<<2);
            En(a,j);
            Fn(j)
        }
        else Cn(a,b);
        i=k;
        return
    }
    function Cn(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0;
        d=a+4|0;
        a=b;
        b=c[d>>2]|0;
        do
        {
            c[b>>2]=0;
            b=(c[d>>2]|0)+4|0;
            c[d>>2]=b;
            a=a+-1|0
        }
        while((a|0)!=0);
        return
    }
    function Dn(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0;
        c[b+12>>2]=0;
        c[b+16>>2]=f;
        do if(d)
        {
            g=f+112|0;
            if(d>>>0<29&(a[g>>0]|0)==0)
            {
                a[g>>0]=1;
                break
            }
            else
            {
                f=Kc(d<<2)|0;
                break
            }
        }
        else f=0;
        while(0);
        c[b>>2]=f;
        e=f+(e<<2)|0;
        c[b+8>>2]=e;
        c[b+4>>2]=e;
        c[b+12>>2]=f+(d<<2);
        return
    }
    function En(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0,f=0,g=0,h=0;
        e=c[a>>2]|0;
        g=a+4|0;
        d=b+4|0;
        f=(c[g>>2]|0)-e|0;
        h=(c[d>>2]|0)+(0-(f>>2)<<2)|0;
        c[d>>2]=h;
        _n(h|0,e|0,f|0)|0;
        f=c[a>>2]|0;
        c[a>>2]=c[d>>2];
        c[d>>2]=f;
        f=b+8|0;
        e=c[g>>2]|0;
        c[g>>2]=c[f>>2];
        c[f>>2]=e;
        f=a+8|0;
        a=b+12|0;
        e=c[f>>2]|0;
        c[f>>2]=c[a>>2];
        c[a>>2]=e;
        c[b>>2]=c[d>>2];
        return
    }
    function Fn(b)
    {
        b=b|0;
        var d=0,e=0,f=0;
        e=c[b+4>>2]|0;
        f=b+8|0;
        d=c[f>>2]|0;
        if((d|0)!=(e|0))
        {
            do d=d+-4|0;
            while((d|0)!=(e|0));
            c[f>>2]=d
        }
        e=c[b>>2]|0;
        do if(e)
        {
            d=c[b+16>>2]|0;
            if((d|0)==(e|0))
            {
                a[d+112>>0]=0;
                break
            }
            else
            {
                Lc(e);
                break
            }
        }
        while(0);
        return
    }
    function Gn(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0;
        if(d>>>0>1073741823)Ic(b);
        e=b+128|0;
        if(d>>>0<29&(a[e>>0]|0)==0)
        {
            a[e>>0]=1;
            e=b+16|0
        }
        else e=Kc(d<<2)|0;
        c[b+4>>2]=e;
        c[b>>2]=e;
        c[b+8>>2]=e+(d<<2);
        return
    }
    function Hn(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0.0,f=0,g=0,h=0,j=0;
        j=i;
        i=i+16|0;
        h=j;
        do if((a|0)!=(b|0))
        {
            f=ob()|0;
            g=c[f>>2]|0;
            c[f>>2]=0;
            e=+Ud(a,h,Jg()|0);
            a=c[f>>2]|0;
            if(!a)c[f>>2]=g;
            if((c[h>>2]|0)!=(b|0))
            {
                c[d>>2]=4;
                e=0.0;
                break
            }
            if((a|0)==34)c[d>>2]=4
        }
        else
        {
            c[d>>2]=4;
            e=0.0
        }
        while(0);
        i=j;
        return +e
    }
    function In(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0.0,f=0,g=0,h=0,j=0;
        j=i;
        i=i+16|0;
        h=j;
        do if((a|0)!=(b|0))
        {
            f=ob()|0;
            g=c[f>>2]|0;
            c[f>>2]=0;
            e=+Ud(a,h,Jg()|0);
            a=c[f>>2]|0;
            if(!a)c[f>>2]=g;
            if((c[h>>2]|0)!=(b|0))
            {
                c[d>>2]=4;
                e=0.0;
                break
            }
            if((a|0)==34)c[d>>2]=4
        }
        else
        {
            c[d>>2]=4;
            e=0.0
        }
        while(0);
        i=j;
        return +e
    }
    function Jn(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0.0,f=0,g=0,h=0,j=0;
        j=i;
        i=i+16|0;
        h=j;
        do if((a|0)==(b|0))
        {
            c[d>>2]=4;
            e=0.0
        }
        else
        {
            f=ob()|0;
            g=c[f>>2]|0;
            c[f>>2]=0;
            e=+Ud(a,h,Jg()|0);
            a=c[f>>2]|0;
            if(!a)c[f>>2]=g;
            if((c[h>>2]|0)!=(b|0))
            {
                c[d>>2]=4;
                e=0.0;
                break
            }
            if((a|0)==34)c[d>>2]=4
        }
        while(0);
        i=j;
        return +e
    }
    function Kn(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0;
        k=i;
        i=i+16|0;
        j=k;
        do if((b|0)!=(d|0))
        {
            if((a[b>>0]|0)==45)
            {
                c[e>>2]=4;
                f=0;
                b=0;
                break
            }
            g=ob()|0;
            h=c[g>>2]|0;
            c[g>>2]=0;
            b=lc(b,j,f,Jg()|0)|0;
            f=c[g>>2]|0;
            if(!f)c[g>>2]=h;
            if((c[j>>2]|0)!=(d|0))
            {
                c[e>>2]=4;
                f=0;
                b=0;
                break
            }
            if((f|0)==34)
            {
                c[e>>2]=4;
                f=-1;
                b=-1
            }
            else f=G
        }
        else
        {
            c[e>>2]=4;
            f=0;
            b=0
        }
        while(0);
        G=f;
        i=k;
        return b|0
    }
    function Ln(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0,l=0;
        l=i;
        i=i+16|0;
        k=l;
        do if((b|0)!=(d|0))
        {
            if((a[b>>0]|0)==45)
            {
                c[e>>2]=4;
                b=0;
                break
            }
            h=ob()|0;
            j=c[h>>2]|0;
            c[h>>2]=0;
            b=lc(b,k,f,Jg()|0)|0;
            f=G;
            g=c[h>>2]|0;
            if(!g)c[h>>2]=j;
            if((c[k>>2]|0)!=(d|0))
            {
                c[e>>2]=4;
                b=0;
                break
            }
            if(f>>>0>0|(f|0)==0&b>>>0>4294967295|(g|0)==34)
            {
                c[e>>2]=4;
                b=-1;
                break
            }
            else break
        }
        else
        {
            c[e>>2]=4;
            b=0
        }
        while(0);
        i=l;
        return b|0
    }
    function Mn(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0,l=0;
        l=i;
        i=i+16|0;
        k=l;
        do if((b|0)!=(d|0))
        {
            if((a[b>>0]|0)==45)
            {
                c[e>>2]=4;
                b=0;
                break
            }
            h=ob()|0;
            j=c[h>>2]|0;
            c[h>>2]=0;
            b=lc(b,k,f,Jg()|0)|0;
            f=G;
            g=c[h>>2]|0;
            if(!g)c[h>>2]=j;
            if((c[k>>2]|0)!=(d|0))
            {
                c[e>>2]=4;
                b=0;
                break
            }
            if(f>>>0>0|(f|0)==0&b>>>0>4294967295|(g|0)==34)
            {
                c[e>>2]=4;
                b=-1;
                break
            }
            else break
        }
        else
        {
            c[e>>2]=4;
            b=0
        }
        while(0);
        i=l;
        return b|0
    }
    function Nn(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0,l=0;
        l=i;
        i=i+16|0;
        k=l;
        do if((b|0)!=(d|0))
        {
            if((a[b>>0]|0)==45)
            {
                c[e>>2]=4;
                b=0;
                break
            }
            h=ob()|0;
            j=c[h>>2]|0;
            c[h>>2]=0;
            b=lc(b,k,f,Jg()|0)|0;
            f=G;
            g=c[h>>2]|0;
            if(!g)c[h>>2]=j;
            if((c[k>>2]|0)!=(d|0))
            {
                c[e>>2]=4;
                b=0;
                break
            }
            if(f>>>0>0|(f|0)==0&b>>>0>65535|(g|0)==34)
            {
                c[e>>2]=4;
                b=-1;
                break
            }
            else
            {
                b=b&65535;
                break
            }
        }
        else
        {
            c[e>>2]=4;
            b=0
        }
        while(0);
        i=l;
        return b|0
    }
    function On(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,j=0,k=0;
        k=i;
        i=i+16|0;
        j=k;
        do if((a|0)!=(b|0))
        {
            g=ob()|0;
            h=c[g>>2]|0;
            c[g>>2]=0;
            a=mc(a,j,e,Jg()|0)|0;
            e=G;
            f=c[g>>2]|0;
            if(!f)c[g>>2]=h;
            if((c[j>>2]|0)!=(b|0))
            {
                c[d>>2]=4;
                e=0;
                a=0;
                break
            }
            if((f|0)==34)
            {
                c[d>>2]=4;
                d=(e|0)>0|(e|0)==0&a>>>0>0;
                G=d?2147483647:-2147483648;
                i=k;
                return (d?-1:0)|0
            }
        }
        else
        {
            c[d>>2]=4;
            e=0;
            a=0
        }
        while(0);
        G=e;
        i=k;
        return a|0
    }
    function Pn(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,j=0,k=0;
        k=i;
        i=i+16|0;
        j=k;
        a:do if((a|0)==(b|0))
        {
            c[d>>2]=4;
            a=0
        }
        else
        {
            g=ob()|0;
            h=c[g>>2]|0;
            c[g>>2]=0;
            a=mc(a,j,e,Jg()|0)|0;
            e=G;
            f=c[g>>2]|0;
            if(!f)c[g>>2]=h;
            if((c[j>>2]|0)!=(b|0))
            {
                c[d>>2]=4;
                a=0;
                break
            }
            do if((f|0)==34)
            {
                c[d>>2]=4;
                if((e|0)>0|(e|0)==0&a>>>0>0)
                {
                    a=2147483647;
                    break a
                }
            }
            else
            {
                if((e|0)<-1|(e|0)==-1&a>>>0<2147483648)
                {
                    c[d>>2]=4;
                    break
                }
                if((e|0)>0|(e|0)==0&a>>>0>2147483647)
                {
                    c[d>>2]=4;
                    a=2147483647;
                    break a
                }
                else break a
            }
            while(0);
            a=-2147483648
        }
        while(0);
        i=k;
        return a|0
    }
    function Qn(a)
    {
        a=a|0;
        return
    }
    function Rn(a)
    {
        a=a|0;
        a=a+4|0;
        c[a>>2]=(c[a>>2]|0)+1;
        return
    }
    function Sn(a)
    {
        a=a|0;
        var b=0,d=0;
        d=a+4|0;
        b=c[d>>2]|0;
        c[d>>2]=b+-1;
        if(!b)
        {
            Mb[c[(c[a>>2]|0)+8>>2]&127](a);
            a=1
        }
        else a=0;
        return a|0
    }
    function Tn(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        Sa(14688)|0;
        if((c[a>>2]|0)==1)do Aa(14720,14688)|0;
        while((c[a>>2]|0)==1);
        if(!(c[a>>2]|0))
        {
            c[a>>2]=1;
            gb(14688)|0;
            Mb[d&127](b);
            Sa(14688)|0;
            c[a>>2]=-1;
            gb(14688)|0;
            wb(14720)|0
        }
        else gb(14688)|0;
        return
    }
    function Un()
    {
    }
    function Vn(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        d=b-d-(c>>>0>a>>>0|0)>>>0;
        return (G=d,a-c>>>0|0)|0
    }
    function Wn(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        c=a+c>>>0;
        return (G=b+d+(c>>>0<a>>>0|0)>>>0,c|0)|0
    }
    function Xn(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,i=0;
        f=b+e|0;
        if((e|0)>=20)
        {
            d=d&255;
            h=b&3;
            i=d|d<<8|d<<16|d<<24;
            g=f&~3;
            if(h)
            {
                h=b+4-h|0;
                while((b|0)<(h|0))
                {
                    a[b>>0]=d;
                    b=b+1|0
                }
            }
            while((b|0)<(g|0))
            {
                c[b>>2]=i;
                b=b+4|0
            }
        }
        while((b|0)<(f|0))
        {
            a[b>>0]=d;
            b=b+1|0
        }
        return b-e|0
    }
    function Yn(b)
    {
        b=b|0;
        var c=0;
        c=b;
        while(a[c>>0]|0)c=c+1|0;
        return c-b|0
    }
    function Zn(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        if((c|0)<32)
        {
            G=b>>>c;
            return a>>>c|(b&(1<<c)-1)<<32-c
        }
        G=0;
        return b>>>c-32|0
    }
    function _n(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0;
        if((e|0)>=4096)return Za(b|0,d|0,e|0)|0;
        f=b|0;
        if((b&3)==(d&3))
        {
            while(b&3)
            {
                if(!e)return f|0;
                a[b>>0]=a[d>>0]|0;
                b=b+1|0;
                d=d+1|0;
                e=e-1|0
            }
            while((e|0)>=4)
            {
                c[b>>2]=c[d>>2];
                b=b+4|0;
                d=d+4|0;
                e=e-4|0
            }
        }
        while((e|0)>0)
        {
            a[b>>0]=a[d>>0]|0;
            b=b+1|0;
            d=d+1|0;
            e=e-1|0
        }
        return f|0
    }
    function $n(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        if((c|0)<32)
        {
            G=b<<c|(a&(1<<c)-1<<32-c)>>>32-c;
            return a<<c
        }
        G=a<<c-32;
        return 0
    }
    function ao(b,c,d)
    {
        b=b|0;
        c=c|0;
        d=d|0;
        var e=0;
        if((c|0)<(b|0)&(b|0)<(c+d|0))
        {
            e=b;
            c=c+d|0;
            b=b+d|0;
            while((d|0)>0)
            {
                b=b-1|0;
                c=c-1|0;
                d=d-1|0;
                a[b>>0]=a[c>>0]|0
            }
            b=e
        }
        else _n(b,c,d)|0;
        return b|0
    }
    function bo(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        if((c|0)<32)
        {
            G=b>>c;
            return a>>>c|(b&(1<<c)-1)<<32-c
        }
        G=(b|0)<0?-1:0;
        return b>>c-32|0
    }
    function co(b)
    {
        b=b|0;
        var c=0;
        c=a[m+(b&255)>>0]|0;
        if((c|0)<8)return c|0;
        c=a[m+(b>>8&255)>>0]|0;
        if((c|0)<8)return c+8|0;
        c=a[m+(b>>16&255)>>0]|0;
        if((c|0)<8)return c+16|0;
        return (a[m+(b>>>24)>>0]|0)+24|0
    }
    function eo(a,b)
    {
        a=a|0;
        b=b|0;
        var c=0,d=0,e=0,f=0;
        f=a&65535;
        e=b&65535;
        c=ca(e,f)|0;
        d=a>>>16;
        a=(c>>>16)+(ca(e,d)|0)|0;
        e=b>>>16;
        b=ca(e,f)|0;
        return (G=(a>>>16)+(ca(e,d)|0)+(((a&65535)+b|0)>>>16)|0,a+b<<16|c&65535|0)|0
    }
    function fo(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,i=0,j=0;
        j=b>>31|((b|0)<0?-1:0)<<1;
        i=((b|0)<0?-1:0)>>31|((b|0)<0?-1:0)<<1;
        f=d>>31|((d|0)<0?-1:0)<<1;
        e=((d|0)<0?-1:0)>>31|((d|0)<0?-1:0)<<1;
        h=Vn(j^a,i^b,j,i)|0;
        g=G;
        a=f^j;
        b=e^i;
        return Vn((ko(h,g,Vn(f^c,e^d,f,e)|0,G,0)|0)^a,G^b,a,b)|0
    }
    function go(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,j=0,k=0,l=0;
        f=i;
        i=i+8|0;
        j=f|0;
        h=b>>31|((b|0)<0?-1:0)<<1;
        g=((b|0)<0?-1:0)>>31|((b|0)<0?-1:0)<<1;
        l=e>>31|((e|0)<0?-1:0)<<1;
        k=((e|0)<0?-1:0)>>31|((e|0)<0?-1:0)<<1;
        a=Vn(h^a,g^b,h,g)|0;
        b=G;
        ko(a,b,Vn(l^d,k^e,l,k)|0,G,j)|0;
        d=Vn(c[j>>2]^h,c[j+4>>2]^g,h,g)|0;
        e=G;
        i=f;
        return (G=e,d)|0
    }
    function ho(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        var e=0,f=0;
        e=a;
        f=c;
        c=eo(e,f)|0;
        a=G;
        return (G=(ca(b,f)|0)+(ca(d,e)|0)+a|a&0,c|0|0)|0
    }
    function io(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        return ko(a,b,c,d,0)|0
    }
    function jo(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        g=i;
        i=i+8|0;
        f=g|0;
        ko(a,b,d,e,f)|0;
        i=g;
        return (G=c[f+4>>2]|0,c[f>>2]|0)|0
    }
    function ko(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        n=a;
        m=b;
        l=m;
        i=d;
        o=e;
        h=o;
        if(!l)
        {
            g=(f|0)!=0;
            if(!h)
            {
                if(g)
                {
                    c[f>>2]=(n>>>0)%(i>>>0);
                    c[f+4>>2]=0
                }
                o=0;
                f=(n>>>0)/(i>>>0)>>>0;
                return (G=o,f)|0
            }
            else
            {
                if(!g)
                {
                    o=0;
                    f=0;
                    return (G=o,f)|0
                }
                c[f>>2]=a|0;
                c[f+4>>2]=b&0;
                o=0;
                f=0;
                return (G=o,f)|0
            }
        }
        g=(h|0)==0;
        do if(i)
        {
            if(!g)
            {
                g=(ea(h|0)|0)-(ea(l|0)|0)|0;
                if(g>>>0<=31)
                {
                    b=g+1|0;
                    h=31-g|0;
                    k=g-31>>31;
                    i=b;
                    j=n>>>(b>>>0)&k|l<<h;
                    k=l>>>(b>>>0)&k;
                    g=0;
                    h=n<<h;
                    break
                }
                if(!f)
                {
                    o=0;
                    f=0;
                    return (G=o,f)|0
                }
                c[f>>2]=a|0;
                c[f+4>>2]=m|b&0;
                o=0;
                f=0;
                return (G=o,f)|0
            }
            g=i-1|0;
            if(g&i)
            {
                h=(ea(i|0)|0)+33-(ea(l|0)|0)|0;
                p=64-h|0;
                b=32-h|0;
                a=b>>31;
                m=h-32|0;
                k=m>>31;
                i=h;
                j=b-1>>31&l>>>(m>>>0)|(l<<b|n>>>(h>>>0))&k;
                k=k&l>>>(h>>>0);
                g=n<<p&a;
                h=(l<<p|n>>>(m>>>0))&a|n<<b&h-33>>31;
                break
            }
            if(f)
            {
                c[f>>2]=g&n;
                c[f+4>>2]=0
            }
            if((i|0)==1)
            {
                f=m|b&0;
                p=a|0|0;
                return (G=f,p)|0
            }
            else
            {
                p=co(i|0)|0;
                f=l>>>(p>>>0)|0;
                p=l<<32-p|n>>>(p>>>0)|0;
                return (G=f,p)|0
            }
        }
        else
        {
            if(g)
            {
                if(f)
                {
                    c[f>>2]=(l>>>0)%(i>>>0);
                    c[f+4>>2]=0
                }
                f=0;
                p=(l>>>0)/(i>>>0)>>>0;
                return (G=f,p)|0
            }
            if(!n)
            {
                if(f)
                {
                    c[f>>2]=0;
                    c[f+4>>2]=(l>>>0)%(h>>>0)
                }
                f=0;
                p=(l>>>0)/(h>>>0)>>>0;
                return (G=f,p)|0
            }
            g=h-1|0;
            if(!(g&h))
            {
                if(f)
                {
                    c[f>>2]=a|0;
                    c[f+4>>2]=g&l|b&0
                }
                f=0;
                p=l>>>((co(h|0)|0)>>>0);
                return (G=f,p)|0
            }
            g=(ea(h|0)|0)-(ea(l|0)|0)|0;
            if(g>>>0<=30)
            {
                k=g+1|0;
                h=31-g|0;
                i=k;
                j=l<<h|n>>>(k>>>0);
                k=l>>>(k>>>0);
                g=0;
                h=n<<h;
                break
            }
            if(!f)
            {
                f=0;
                p=0;
                return (G=f,p)|0
            }
            c[f>>2]=a|0;
            c[f+4>>2]=m|b&0;
            f=0;
            p=0;
            return (G=f,p)|0
        }
        while(0);
        if(!i)
        {
            m=h;
            i=0;
            a=0
        }
        else
        {
            d=d|0|0;
            b=o|e&0;
            l=Wn(d|0,b|0,-1,-1)|0;
            m=G;
            a=0;
            do
            {
                n=h;
                h=g>>>31|h<<1;
                g=a|g<<1;
                n=j<<1|n>>>31|0;
                e=j>>>31|k<<1|0;
                Vn(l,m,n,e)|0;
                p=G;
                o=p>>31|((p|0)<0?-1:0)<<1;
                a=o&1;
                j=Vn(n,e,o&d,(((p|0)<0?-1:0)>>31|((p|0)<0?-1:0)<<1)&b)|0;
                k=G;
                i=i-1|0
            }
            while((i|0)!=0);
            m=h;
            i=0
        }
        h=0;
        if(f)
        {
            c[f>>2]=j;
            c[f+4>>2]=k
        }
        f=(g|0)>>>31|(m|h)<<1|(h<<1|g>>>31)&0|i;
        p=(g<<1|0>>>31)&-2|a;
        return (G=f,p)|0
    }
    function lo(a,b,c,d,e,f,g,h)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        return Ib[a&7](b|0,c|0,d|0,e|0,f|0,g|0,h|0)|0
    }
    function mo(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        return Jb[a&31](b|0,c|0,d|0)|0
    }
    function no(a,b,c,d,e,f)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        Kb[a&3](b|0,c|0,d|0,e|0,f|0)
    }
    function oo(a,b,c,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=+g;
        return Lb[a&3](b|0,c|0,d|0,e|0,f|0,+g)|0
    }
    function po(a,b)
    {
        a=a|0;
        b=b|0;
        Mb[a&127](b|0)
    }
    function qo(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        Nb[a&63](b|0,c|0)
    }
    function ro(a,b,c,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        return Ob[a&63](b|0,c|0,d|0,e|0,f|0,g|0)|0
    }
    function so(a,b,c,d,e,f)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=+f;
        return Pb[a&7](b|0,c|0,d|0,e|0,+f)|0
    }
    function to(a,b)
    {
        a=a|0;
        b=b|0;
        return Qb[a&63](b|0)|0
    }
    function uo(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        Rb[a&0](b|0,c|0,d|0)
    }
    function vo(a)
    {
        a=a|0;
        Sb[a&3]()
    }
    function wo(a,b,c,d,e,f,g,h,i)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        i=i|0;
        return Tb[a&15](b|0,c|0,d|0,e|0,f|0,g|0,h|0,i|0)|0
    }
    function xo(a,b,c,d,e)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        return Ub[a&7](b|0,c|0,d|0,e|0)|0
    }
    function yo(a,b,c,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        Vb[a&7](b|0,c|0,d|0,e|0,f|0,g|0)
    }
    function zo(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        return Wb[a&15](b|0,c|0)|0
    }
    function Ao(a,b,c,d,e,f)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        return Xb[a&31](b|0,c|0,d|0,e|0,f|0)|0
    }
    function Bo(a,b,c,d,e)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        Yb[a&7](b|0,c|0,d|0,e|0)
    }
    function Co(a,b,c,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        fa(0);
        return 0
    }
    function Do(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        fa(1);
        return 0
    }
    function Eo(a,b,c,d,e)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        fa(2)
    }
    function Fo(a,b,c,d,e,f)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=+f;
        fa(3);
        return 0
    }
    function Go(a)
    {
        a=a|0;
        fa(4)
    }
    function Ho(a,b)
    {
        a=a|0;
        b=b|0;
        fa(5)
    }
    function Io(a,b,c,d,e,f)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        fa(6);
        return 0
    }
    function Jo(a,b,c,d,e)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=+e;
        fa(7);
        return 0
    }
    function Ko(a)
    {
        a=a|0;
        fa(8);
        return 0
    }
    function Lo(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        fa(9)
    }
    function Mo()
    {
        fa(10)
    }
    function No(a,b,c,d,e,f,g,h)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        fa(11);
        return 0
    }
    function Oo(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        fa(12);
        return 0
    }
    function Po(a,b,c,d,e,f)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        f=f|0;
        fa(13)
    }
    function Qo(a,b)
    {
        a=a|0;
        b=b|0;
        fa(14);
        return 0
    }
    function Ro(a,b,c,d,e)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        e=e|0;
        fa(15);
        return 0
    }
    function So(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        fa(16)
    }
    function Zb(a)
    {
        a=a|0;
        var b=0;
        b=i;
        i=i+a|0;
        i=i+15&-16;
        return b|0
    }
    function _b()
    {
        return i|0
    }
    function $b(a)
    {
        a=a|0;
        i=a
    }
    function ac(a,b)
    {
        a=a|0;
        b=b|0;
        i=a;
        j=b
    }
    function bc(a,b)
    {
        a=a|0;
        b=b|0;
        if(!r)
        {
            r=a;
            s=b
        }
    }
    function cc(b)
    {
        b=b|0;
        a[k>>0]=a[b>>0];
        a[k+1>>0]=a[b+1>>0];
        a[k+2>>0]=a[b+2>>0];
        a[k+3>>0]=a[b+3>>0]
    }
    function dc(b)
    {
        b=b|0;
        a[k>>0]=a[b>>0];
        a[k+1>>0]=a[b+1>>0];
        a[k+2>>0]=a[b+2>>0];
        a[k+3>>0]=a[b+3>>0];
        a[k+4>>0]=a[b+4>>0];
        a[k+5>>0]=a[b+5>>0];
        a[k+6>>0]=a[b+6>>0];
        a[k+7>>0]=a[b+7>>0]
    }
    function ec(a)
    {
        a=a|0;
        G=a
    }
    function fc()
    {
        return G|0
    }
    function gc()
    {
        var a=0,b=0,d=0.0,e=0.0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0;
        o=i;
        i=i+32|0;
        l=o+28|0;
        m=o+16|0;
        f=o;
        c[m>>2]=0;
        n=m+4|0;
        c[n>>2]=0;
        c[m+8>>2]=0;
        b=m+8|0;
        g=Kc(16e8)|0;
        c[m>>2]=g;
        c[n>>2]=g;
        c[b>>2]=g+16e8;
        g=f+8|0;
        j=0;
        do
        {
            d=+(j|0)*2.0/9999.0+-1.0;
            k=0;
            do
            {
                h[f>>3]=d;
                h[g>>3]=+(k|0)*2.0/9999.0+-1.0;
                a=c[n>>2]|0;
                if((a|0)==(c[b>>2]|0))hc(m,f);
                else
                {
                    c[a>>2]=c[f>>2];
                    c[a+4>>2]=c[f+4>>2];
                    c[a+8>>2]=c[f+8>>2];
                    c[a+12>>2]=c[f+12>>2];
                    c[n>>2]=a+16
                }
                k=k+1|0
            }
            while((k|0)<1e4);
            j=j+1|0
        }
        while((j|0)<1e4);
        a=c[m>>2]|0;
        f=c[n>>2]|0;
        if((a|0)==(f|0))
        {
            e=0.0;
            d=0.0
        }
        else
        {
            g=(f+-16-a|0)>>>4;
            b=0;
            do
            {
                d=+h[a>>3];
                e=+h[a+8>>3];
                b=(d*d+e*e<1.0&1)+b|0;
                a=a+16|0
            }
            while((a|0)!=(f|0));
            a=b;
            e=+(g+1|0);
            d=+(a|0)*4.0
        }
        a=ag(ic(3072,8,8)|0,d/e)|0;
        c[l>>2]=ff(a+(c[(c[a>>2]|0)+-12>>2]|0)|0)|0;
        j=tk(l,6584)|0;
        j=Wb[c[(c[j>>2]|0)+28>>2]&15](j,10)|0;
        rk(l);
        bg(a,j)|0;
        Qf(a)|0;
        a=c[m>>2]|0;
        if(!a)
        {
            i=o;
            return 0
        }
        b=c[n>>2]|0;
        if((b|0)!=(a|0))c[n>>2]=b+(~((b+-16-a|0)>>>4)<<4);
        Lc(a);
        i=o;
        return 0
    }
    function hc(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0;
        i=a+4|0;
        j=c[a>>2]|0;
        k=j;
        e=((c[i>>2]|0)-k>>4)+1|0;
        if(e>>>0>268435455)Ic(a);
        l=a+8|0;
        f=j;
        d=(c[l>>2]|0)-f|0;
        if(d>>4>>>0<134217727)
        {
            d=d>>3;
            d=d>>>0<e>>>0?e:d;
            f=(c[i>>2]|0)-f|0;
            e=f>>4;
            if(!d)
            {
                h=0;
                g=0;
                d=f
            }
            else m=6
        }
        else
        {
            f=(c[i>>2]|0)-f|0;
            d=268435455;
            e=f>>4;
            m=6
        }
        if((m|0)==6)
        {
            h=d;
            g=Kc(d<<4)|0;
            d=f
        }
        m=g+(e<<4)|0;
        c[m>>2]=c[b>>2];
        c[m+4>>2]=c[b+4>>2];
        c[m+8>>2]=c[b+8>>2];
        c[m+12>>2]=c[b+12>>2];
        _n(g|0,j|0,d|0)|0;
        c[a>>2]=g;
        c[i>>2]=g+(e+1<<4);
        c[l>>2]=g+(h<<4);
        if(!k)return;
        Lc(k);
        return
    }
    function ic(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0;
        n=i;
        i=i+16|0;
        h=n+12|0;
        m=n;
        j=n+8|0;
        _f(m,b);
        if(!(a[m>>0]|0))
        {
            $f(m);
            i=n;
            return b|0
        }
        f=c[(c[b>>2]|0)+-12>>2]|0;
        c[j>>2]=c[b+(f+24)>>2];
        l=b+f|0;
        k=c[b+(f+4)>>2]|0;
        g=d+e|0;
        f=b+(f+76)|0;
        e=c[f>>2]|0;
        if((e|0)==-1)
        {
            c[h>>2]=ff(l)|0;
            e=tk(h,6584)|0;
            e=Wb[c[(c[e>>2]|0)+28>>2]&15](e,32)|0;
            rk(h);
            e=e<<24>>24;
            c[f>>2]=e
        }
        c[h>>2]=c[j>>2];
        if(kc(h,d,(k&176|0)==32?g:d,g,l,e&255)|0)
        {
            $f(m);
            i=n;
            return b|0
        }
        d=c[(c[b>>2]|0)+-12>>2]|0;
        cf(b+d|0,c[b+(d+16)>>2]|5);
        $f(m);
        i=n;
        return b|0
    }
    function jc(a)
    {
        a=a|0;
        Ya(a|0)|0;
        Sc()
    }
    function kc(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        o=i;
        i=i+16|0;
        m=o;
        n=c[b>>2]|0;
        if(!n)
        {
            b=0;
            i=o;
            return b|0
        }
        p=d;
        k=f-p|0;
        l=g+12|0;
        j=c[l>>2]|0;
        k=(j|0)>(k|0)?j-k|0:0;
        j=e;
        g=j-p|0;
        if((g|0)>0?(Jb[c[(c[n>>2]|0)+48>>2]&31](n,d,g)|0)!=(g|0):0)
        {
            c[b>>2]=0;
            p=0;
            i=o;
            return p|0
        }
        do if((k|0)>0)
        {
            Je(m,k,h);
            if((Jb[c[(c[n>>2]|0)+48>>2]&31](n,(a[m>>0]&1)==0?m+1|0:c[m+8>>2]|0,k)|0)==(k|0))
            {
                Ke(m);
                break
            }
            c[b>>2]=0;
            Ke(m);
            p=0;
            i=o;
            return p|0
        }
        while(0);
        f=f-j|0;
        if((f|0)>0?(Jb[c[(c[n>>2]|0)+48>>2]&31](n,e,f)|0)!=(f|0):0)
        {
            c[b>>2]=0;
            p=0;
            i=o;
            return p|0
        }
        c[l>>2]=0;
        p=n;
        i=o;
        return p|0
    }
    function lc(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        b=Vd(a,b,c)|0;
        return b|0
    }
    function mc(a,b,c,d)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        d=d|0;
        b=Wd(a,b,c)|0;
        return b|0
    }
    function nc(a,b)
    {
        a=a|0;
        b=b|0;
        return (a+-48|0)>>>0<10|0
    }
    function oc(a,b)
    {
        a=a|0;
        b=b|0;
        return yd(a)|0
    }
    function pc(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        return qc(0,a,b,(c|0)!=0?c:232)|0
    }
    function qc(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0,l=0;
        l=i;
        i=i+16|0;
        g=l;
        c[g>>2]=b;
        k=(f|0)==0?240:f;
        f=c[k>>2]|0;
        a:do if(!d)
        {
            if(!f)
            {
                k=0;
                i=l;
                return k|0
            }
        }
        else
        {
            if(!b)
            {
                c[g>>2]=g;
                j=g
            }
            else j=b;
            if(!e)
            {
                k=-2;
                i=l;
                return k|0
            }
            do if(!f)
            {
                f=a[d>>0]|0;
                g=f&255;
                if(f<<24>>24<=-1)
                {
                    f=g+-194|0;
                    if(f>>>0>50)break a;
                    f=c[24+(f<<2)>>2]|0;
                    g=e+-1|0;
                    if(!g)break;
                    else
                    {
                        d=d+1|0;
                        h=11;
                        break
                    }
                }
                else
                {
                    c[j>>2]=g;
                    k=f<<24>>24!=0&1;
                    i=l;
                    return k|0
                }
            }
            else
            {
                g=e;
                h=11
            }
            while(0);
            b:do if((h|0)==11)
            {
                b=a[d>>0]|0;
                h=(b&255)>>>3;
                if((h+-16|h+(f>>26))>>>0>7)break a;
                while(1)
                {
                    d=d+1|0;
                    f=(b&255)+-128|f<<6;
                    g=g+-1|0;
                    if((f|0)>=0)break;
                    if(!g)break b;
                    b=a[d>>0]|0;
                    if((b&-64)<<24>>24!=-128)break a
                }
                c[k>>2]=0;
                c[j>>2]=f;
                k=e-g|0;
                i=l;
                return k|0
            }
            while(0);
            c[k>>2]=f;
            k=-2;
            i=l;
            return k|0
        }
        while(0);
        c[k>>2]=0;
        c[(ob()|0)>>2]=84;
        k=-1;
        i=l;
        return k|0
    }
    function rc(a)
    {
        a=a|0;
        if(!a)a=1;
        else a=(c[a>>2]|0)==0;
        return a&1|0
    }
    function sc(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        p=i;
        i=i+1040|0;
        m=p+8|0;
        o=p;
        k=c[b>>2]|0;
        c[o>>2]=k;
        n=(a|0)!=0;
        e=n?e:256;
        a=n?a:m;
        g=k;
        a:do if((e|0)!=0&(k|0)!=0)
        {
            l=e;
            k=g;
            e=0;
            while(1)
            {
                g=d>>>2;
                h=g>>>0>=l>>>0;
                if(!(d>>>0>131|h))
                {
                    j=l;
                    g=k;
                    break a
                }
                g=h?l:g;
                d=d-g|0;
                g=tc(a,o,g,f)|0;
                if((g|0)==-1)
                {
                    e=d;
                    break
                }
                h=(a|0)==(m|0);
                k=h?0:g;
                j=l-k|0;
                a=h?a:a+(g<<2)|0;
                e=g+e|0;
                g=c[o>>2]|0;
                if((l|0)!=(k|0)&(g|0)!=0)
                {
                    l=j;
                    k=g
                }
                else break a
            }
            d=e;
            j=0;
            g=c[o>>2]|0;
            e=-1
        }
        else
        {
            j=e;
            e=0
        }
        while(0);
        b:do if((g|0)!=0?(j|0)!=0&(d|0)!=0:0)
        {
            h=g;
            g=a;
            while(1)
            {
                a=qc(g,h,d,f)|0;
                if((a+2|0)>>>0<3)break;
                h=(c[o>>2]|0)+a|0;
                c[o>>2]=h;
                j=j+-1|0;
                e=e+1|0;
                if(!((j|0)!=0&(d|0)!=(a|0)))break b;
                else
                {
                    d=d-a|0;
                    g=g+4|0
                }
            }
            if(!a)
            {
                c[o>>2]=0;
                break
            }
            else if((a|0)==-1)
            {
                e=-1;
                break
            }
            else
            {
                c[f>>2]=0;
                break
            }
        }
        while(0);
        if(!n)
        {
            i=p;
            return e|0
        }
        c[b>>2]=c[o>>2];
        i=p;
        return e|0
    }
    function tc(b,e,f,g)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,i=0,j=0,k=0,l=0;
        h=c[e>>2]|0;
        if((g|0)!=0?(i=c[g>>2]|0,(i|0)!=0):0)if(!b)
        {
            g=f;
            j=h;
            l=16
        }
        else
        {
            c[g>>2]=0;
            g=f;
            j=i;
            k=h;
            l=37
        }
        else if(!b)
        {
            g=f;
            l=7
        }
        else
        {
            g=f;
            l=6
        }
        a:while(1)if((l|0)==6)
        {
            if(!g)
            {
                l=26;
                break
            }
            while(1)
            {
                i=a[h>>0]|0;
                do if(((i&255)+-1|0)>>>0<127?g>>>0>4&(h&3|0)==0:0)
                {
                    j=b;
                    while(1)
                    {
                        i=c[h>>2]|0;
                        if((i+-16843009|i)&-2139062144)
                        {
                            b=j;
                            l=32;
                            break
                        }
                        c[j>>2]=i&255;
                        c[j+4>>2]=d[h+1>>0];
                        c[j+8>>2]=d[h+2>>0];
                        b=h+4|0;
                        i=j+16|0;
                        c[j+12>>2]=d[h+3>>0];
                        g=g+-4|0;
                        if(g>>>0>4)
                        {
                            j=i;
                            h=b
                        }
                        else
                        {
                            h=b;
                            l=31;
                            break
                        }
                    }
                    if((l|0)==31)
                    {
                        b=i;
                        i=a[h>>0]|0;
                        break
                    }
                    else if((l|0)==32)
                    {
                        i=i&255;
                        break
                    }
                }
                while(0);
                i=i&255;
                if((i+-1|0)>>>0>=127)break;
                h=h+1|0;
                c[b>>2]=i;
                g=g+-1|0;
                if(!g)
                {
                    l=26;
                    break a
                }
                else b=b+4|0
            }
            i=i+-194|0;
            if(i>>>0>50)
            {
                l=48;
                break
            }
            j=c[24+(i<<2)>>2]|0;
            k=h+1|0;
            l=37;
            continue
        }
        else if((l|0)==7)
        {
            i=a[h>>0]|0;
            if(((i&255)+-1|0)>>>0<127?(h&3|0)==0:0)
            {
                i=c[h>>2]|0;
                if(!((i+-16843009|i)&-2139062144))do
                    {
                    h=h+4|0;
                    g=g+-4|0;
                    i=c[h>>2]|0
                }
                while(((i+-16843009|i)&-2139062144|0)==0);
                i=i&255
            }
            i=i&255;
            if((i+-1|0)>>>0<127)
            {
                g=g+-1|0;
                h=h+1|0;
                l=7;
                continue
            }
            i=i+-194|0;
            if(i>>>0>50)
            {
                l=48;
                break
            }
            i=c[24+(i<<2)>>2]|0;
            j=h+1|0;
            l=16;
            continue
        }
        else if((l|0)==16)
        {
            l=(d[j>>0]|0)>>>3;
            if((l+-16|l+(i>>26))>>>0>7)
            {
                l=17;
                break
            }
            h=j+1|0;
            if(i&33554432)
            {
                if((a[h>>0]&-64)<<24>>24!=-128)
                {
                    l=20;
                    break
                }
                h=j+2|0;
                if(i&524288)
                {
                    if((a[h>>0]&-64)<<24>>24!=-128)
                    {
                        l=23;
                        break
                    }
                    h=j+3|0
                }
            }
            g=g+-1|0;
            l=7;
            continue
        }
        else if((l|0)==37)
        {
            i=d[k>>0]|0;
            l=i>>>3;
            if((l+-16|l+(j>>26))>>>0>7)
            {
                l=38;
                break
            }
            h=k+1|0;
            j=i+-128|j<<6;
            if((j|0)<0)
            {
                i=d[h>>0]|0;
                if((i&192|0)!=128)
                {
                    l=41;
                    break
                }
                h=k+2|0;
                i=i+-128|j<<6;
                if((i|0)<0)
                {
                    h=d[h>>0]|0;
                    if((h&192|0)!=128)
                    {
                        l=44;
                        break
                    }
                    i=h+-128|i<<6;
                    h=k+3|0
                }
            }
            else i=j;
            c[b>>2]=i;
            b=b+4|0;
            g=g+-1|0;
            l=6;
            continue
        }
        if((l|0)==17)
        {
            h=j+-1|0;
            l=47
        }
        else if((l|0)==20)
        {
            h=j+-1|0;
            l=47
        }
        else if((l|0)==23)
        {
            h=j+-1|0;
            l=47
        }
        else if((l|0)==26)
        {
            c[e>>2]=h;
            l=f;
            return l|0
        }
        else if((l|0)==38)
        {
            i=j;
            h=k+-1|0;
            l=47
        }
        else if((l|0)==41)
        {
            g=b;
            h=k+-1|0
        }
        else if((l|0)==44)
        {
            g=b;
            h=k+-1|0
        }
        if((l|0)==47)if(!i)l=48;
        else g=b;
        if((l|0)==48)if(!(a[h>>0]|0))
        {
            if(b)
            {
                c[b>>2]=0;
                c[e>>2]=0
            }
            l=f-g|0;
            return l|0
        }
        else g=b;
        c[(ob()|0)>>2]=84;
        if(!g)
        {
            l=-1;
            return l|0
        }
        c[e>>2]=h;
        l=-1;
        return l|0
    }
    function uc(b,e,f)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0;
        k=i;
        i=i+16|0;
        g=k;
        c[g>>2]=b;
        if(!e)
        {
            e=0;
            i=k;
            return e|0
        }
        do if(f)
        {
            if(!b)
            {
                c[g>>2]=g;
                j=g
            }
            else j=b;
            g=a[e>>0]|0;
            b=g&255;
            if(g<<24>>24>-1)
            {
                c[j>>2]=b;
                e=g<<24>>24!=0&1;
                i=k;
                return e|0
            }
            g=b+-194|0;
            if(g>>>0<=50)
            {
                b=e+1|0;
                h=c[24+(g<<2)>>2]|0;
                if(f>>>0<4?(h&-2147483648>>>((f*6|0)+-6|0)|0)!=0:0)break;
                g=d[b>>0]|0;
                f=g>>>3;
                if((f+-16|f+(h>>26))>>>0<=7)
                {
                    g=g+-128|h<<6;
                    if((g|0)>=0)
                    {
                        c[j>>2]=g;
                        e=2;
                        i=k;
                        return e|0
                    }
                    b=d[e+2>>0]|0;
                    if((b&192|0)==128)
                    {
                        b=b+-128|g<<6;
                        if((b|0)>=0)
                        {
                            c[j>>2]=b;
                            e=3;
                            i=k;
                            return e|0
                        }
                        g=d[e+3>>0]|0;
                        if((g&192|0)==128)
                        {
                            c[j>>2]=g+-128|b<<6;
                            e=4;
                            i=k;
                            return e|0
                        }
                    }
                }
            }
        }
        while(0);
        c[(ob()|0)>>2]=84;
        e=-1;
        i=k;
        return e|0
    }
    function vc(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0;
        n=i;
        i=i+272|0;
        k=n+8|0;
        m=n;
        j=c[b>>2]|0;
        c[m>>2]=j;
        l=(a|0)!=0;
        f=l?e:256;
        e=l?a:k;
        a=j;
        a:do if((f|0)!=0&(j|0)!=0)
        {
            j=f;
            g=a;
            f=0;
            while(1)
            {
                a=d>>>0>=j>>>0;
                if(!(a|d>>>0>32))
                {
                    h=j;
                    a=g;
                    break a
                }
                a=a?j:d;
                d=d-a|0;
                a=wc(e,m,a,0)|0;
                if((a|0)==-1)
                {
                    f=d;
                    break
                }
                o=(e|0)==(k|0);
                g=o?0:a;
                h=j-g|0;
                e=o?e:e+a|0;
                f=a+f|0;
                a=c[m>>2]|0;
                if((j|0)!=(g|0)&(a|0)!=0)
                {
                    j=h;
                    g=a
                }
                else break a
            }
            d=f;
            h=0;
            a=c[m>>2]|0;
            f=-1
        }
        else
        {
            h=f;
            f=0
        }
        while(0);
        b:do if((a|0)!=0?(h|0)!=0&(d|0)!=0:0)
        {
            g=a;
            a=e;
            while(1)
            {
                e=Md(a,c[g>>2]|0,0)|0;
                if((e+1|0)>>>0<2)break;
                g=(c[m>>2]|0)+4|0;
                c[m>>2]=g;
                d=d+-1|0;
                f=f+1|0;
                if(!((h|0)!=(e|0)&(d|0)!=0))break b;
                else
                {
                    h=h-e|0;
                    a=a+e|0
                }
            }
            if(!e)c[m>>2]=0;
            else f=-1
        }
        while(0);
        if(!l)
        {
            i=n;
            return f|0
        }
        c[b>>2]=c[m>>2];
        i=n;
        return f|0
    }
    function wc(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;
        o=i;
        i=i+16|0;
        n=o;
        if(!b)
        {
            g=c[d>>2]|0;
            f=c[g>>2]|0;
            if(!f)
            {
                d=0;
                i=o;
                return d|0
            }
            else h=0;
            while(1)
            {
                if(f>>>0>127)
                {
                    f=Md(n,f,0)|0;
                    if((f|0)==-1)
                    {
                        m=-1;
                        j=26;
                        break
                    }
                }
                else f=1;
                h=f+h|0;
                g=g+4|0;
                f=c[g>>2]|0;
                if(!f)
                {
                    m=h;
                    j=26;
                    break
                }
            }
            if((j|0)==26)
            {
                i=o;
                return m|0
            }
        }
        a:do if(e>>>0>3)
        {
            f=e;
            h=c[d>>2]|0;
            while(1)
            {
                g=c[h>>2]|0;
                if((g+-1|0)>>>0>126)
                {
                    if(!g)
                    {
                        k=b;
                        l=f;
                        break
                    }
                    g=Md(b,g,0)|0;
                    if((g|0)==-1)
                    {
                        m=-1;
                        j=26;
                        break
                    }
                    b=b+g|0;
                    f=f-g|0
                }
                else
                {
                    a[b>>0]=g;
                    b=b+1|0;
                    f=f+-1|0;
                    h=c[d>>2]|0
                }
                h=h+4|0;
                c[d>>2]=h;
                if(f>>>0<=3)break a
            }
            if((j|0)==26)
            {
                i=o;
                return m|0
            }
            a[k>>0]=0;
            c[d>>2]=0;
            d=e-l|0;
            i=o;
            return d|0
        }
        else f=e;
        while(0);
        if(!f)
        {
            d=e;
            i=o;
            return d|0
        }
        g=c[d>>2]|0;
        while(1)
        {
            h=c[g>>2]|0;
            if((h+-1|0)>>>0>126)
            {
                if(!h)
                {
                    p=b;
                    q=f;
                    j=19;
                    break
                }
                h=Md(n,h,0)|0;
                if((h|0)==-1)
                {
                    m=-1;
                    j=26;
                    break
                }
                if(f>>>0<h>>>0)
                {
                    r=f;
                    j=22;
                    break
                }
                Md(b,c[g>>2]|0,0)|0;
                b=b+h|0;
                f=f-h|0
            }
            else
            {
                a[b>>0]=h;
                b=b+1|0;
                f=f+-1|0;
                g=c[d>>2]|0
            }
            g=g+4|0;
            c[d>>2]=g;
            if(!f)
            {
                m=e;
                j=26;
                break
            }
        }
        if((j|0)==19)
        {
            a[p>>0]=0;
            c[d>>2]=0;
            d=e-q|0;
            i=o;
            return d|0
        }
        else if((j|0)==22)
        {
            d=e-r|0;
            i=o;
            return d|0
        }
        else if((j|0)==26)
        {
            i=o;
            return m|0
        }
        return 0
    }
    function xc(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0;
        e=a+84|0;
        g=c[e>>2]|0;
        h=d+256|0;
        f=Xd(g,0,h)|0;
        f=(f|0)==0?h:f-g|0;
        d=f>>>0<d>>>0?f:d;
        _n(b|0,g|0,d|0)|0;
        c[a+4>>2]=g+d;
        b=g+f|0;
        c[a+8>>2]=b;
        c[e>>2]=b;
        return d|0
    }
    function yc(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0;
        e=i;
        i=i+16|0;
        f=e;
        c[f>>2]=d;
        d=Bc(a,b,f)|0;
        i=e;
        return d|0
    }
    function zc(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0;
        j=i;
        i=i+16|0;
        e=j;
        f=$d(240)|0;
        do if(f)
        {
            c[e>>2]=c[d>>2];
            e=Td(f,240,b,e)|0;
            if(e>>>0<240)
            {
                b=be(f,e+1|0)|0;
                c[a>>2]=(b|0)!=0?b:f;
                break
            }
            ae(f);
            if((e|0)>=0?(h=e+1|0,g=$d(h)|0,c[a>>2]=g,(g|0)!=0):0)e=Td(g,h,b,d)|0;
            else e=-1
        }
        else e=-1;
        while(0);
        i=j;
        return e|0
    }
    function Ac(e,f,j)
    {
        e=e|0;
        f=f|0;
        j=j|0;
        var k=0,l=0,m=0,n=0,o=0,p=0.0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0;
        P=i;
        i=i+304|0;
        I=P+16|0;
        K=P;
        J=P+33|0;
        L=P+8|0;
        z=P+32|0;
        k=a[f>>0]|0;
        if(!(k<<24>>24))
        {
            O=0;
            i=P;
            return O|0
        }
        M=e+4|0;
        N=e+100|0;
        C=e+108|0;
        D=e+8|0;
        E=J+10|0;
        F=J+33|0;
        H=K+4|0;
        A=J+46|0;
        B=J+94|0;
        q=k;
        k=0;
        t=0;
        m=0;
        l=0;
        a:while(1)
            {
                b:do if(!(xd(q&255)|0))
                {
                    o=(a[f>>0]|0)==37;
                    c:do if(o)
                    {
                        q=f+1|0;
                        n=a[q>>0]|0;
                        do if(n<<24>>24==37)break c;
                        else if(n<<24>>24==42)
                        {
                            n=0;
                            q=f+2|0
                        }
                        else
                        {
                            o=(n&255)+-48|0;
                            if(o>>>0<10?(a[f+2>>0]|0)==36:0)
                            {
                                c[I>>2]=c[j>>2];
                                while(1)
                                {
                                    y=(c[I>>2]|0)+(4-1)&~(4-1);
                                    n=c[y>>2]|0;
                                    c[I>>2]=y+4;
                                    if(o>>>0>1)o=o+-1|0;
                                    else break
                                }
                                q=f+3|0;
                                break
                            }
                            y=(c[j>>2]|0)+(4-1)&~(4-1);
                            n=c[y>>2]|0;
                            c[j>>2]=y+4
                        }
                        while(0);
                        f=a[q>>0]|0;
                        o=f&255;
                        if((o+-48|0)>>>0<10)
                        {
                            f=0;
                            while(1)
                            {
                                r=(f*10|0)+-48+o|0;
                                q=q+1|0;
                                f=a[q>>0]|0;
                                o=f&255;
                                if((o+-48|0)>>>0>=10)break;
                                else f=r
                            }
                        }
                        else r=0;
                        if(f<<24>>24==109)
                        {
                            q=q+1|0;
                            s=a[q>>0]|0;
                            f=(n|0)!=0&1;
                            m=0;
                            l=0
                        }
                        else
                        {
                            s=f;
                            f=0
                        }
                        o=q+1|0;
                        switch(s&255|0)
                        {
                            case 116:case 122:
                                {
                                    q=o;
                                    o=1;
                                    break
                                }
                            case 76:
                                {
                                    q=o;
                                    o=2;
                                    break
                                }
                            case 104:
                                {
                                    y=(a[o>>0]|0)==104;
                                    q=y?q+2|0:o;
                                    o=y?-2:-1;
                                    break
                                }
                            case 108:
                                {
                                    y=(a[o>>0]|0)==108;
                                    q=y?q+2|0:o;
                                    o=y?3:1;
                                    break
                                }
                            case 110:case 112:case 67:case 83:case 91:case 99:case 115:case 88:case 71:case 70:case 69:case 65:case 103:case 102:case 101:case 97:case 120:case 117:case 111:case 105:case 100:
                                {
                                    o=0;
                                    break
                                }
                            case 106:
                                {
                                    q=o;
                                    o=3;
                                    break
                                }
                            default:
                                {
                                    O=164;
                                    break a
                                }
                        }
                        s=d[q>>0]|0;
                        v=(s&47|0)==3;
                        s=v?s|32:s;
                        v=v?1:o;
                        if((s|0)==99)
                        {
                            y=t;
                            x=(r|0)<1?1:r
                        }
                        else if((s|0)==110)
                        {
                            if(!n)
                            {
                                f=q;
                                s=t;
                                break b
                            }
                            switch(v|0)
                            {
                                case 3:
                                    {
                                        f=n;
                                        c[f>>2]=t;
                                        c[f+4>>2]=((t|0)<0)<<31>>31;
                                        f=q;
                                        s=t;
                                        break b
                                    }
                                case -1:
                                    {
                                        b[n>>1]=t;
                                        f=q;
                                        s=t;
                                        break b
                                    }
                                case 0:
                                    {
                                        c[n>>2]=t;
                                        f=q;
                                        s=t;
                                        break b
                                    }
                                case -2:
                                    {
                                        a[n>>0]=t;
                                        f=q;
                                        s=t;
                                        break b
                                    }
                                case 1:
                                    {
                                        c[n>>2]=t;
                                        f=q;
                                        s=t;
                                        break b
                                    }
                                default:
                                    {
                                        f=q;
                                        s=t;
                                        break b
                                    }
                            }
                        }
                        else if((s|0)==91)
                        {
                            y=t;
                            x=r
                        }
                        else
                        {
                            Bd(e,0);
                            do
                            {
                                o=c[M>>2]|0;
                                if(o>>>0<(c[N>>2]|0)>>>0)
                                {
                                    c[M>>2]=o+1;
                                    o=d[o>>0]|0
                                }
                                else o=Cd(e)|0
                            }
                            while((xd(o)|0)!=0);
                            o=c[M>>2]|0;
                            if(c[N>>2]|0)
                            {
                                o=o+-1|0;
                                c[M>>2]=o
                            }
                            y=(c[C>>2]|0)+t+o-(c[D>>2]|0)|0;
                            x=r
                        }
                        Bd(e,x);
                        o=c[M>>2]|0;
                        r=c[N>>2]|0;
                        if(o>>>0<r>>>0)c[M>>2]=o+1;
                        else
                        {
                            if((Cd(e)|0)<0)
                            {
                                O=164;
                                break a
                            }
                            r=c[N>>2]|0
                        }
                        if(r)c[M>>2]=(c[M>>2]|0)+-1;
                        d:do switch(s|0)
                            {
                            case 111:
                                {
                                    o=8;
                                    O=146;
                                    break
                                }
                            case 117:case 100:
                                {
                                    o=10;
                                    O=146;
                                    break
                                }
                            case 105:
                                {
                                    o=0;
                                    O=146;
                                    break
                                }
                            case 120:case 88:case 112:
                                {
                                    o=16;
                                    O=146;
                                    break
                                }
                            case 71:case 103:case 70:case 102:case 69:case 101:case 65:case 97:
                                {
                                    p=+Ad(e,v,0);
                                    if((c[C>>2]|0)==((c[D>>2]|0)-(c[M>>2]|0)|0))break a;
                                    if(n)if((v|0)==1)
                                    {
                                        h[n>>3]=p;
                                        f=q;
                                        break d
                                    }
                                    else if(!v)
                                    {
                                        g[n>>2]=p;
                                        f=q;
                                        break d
                                    }
                                    else if((v|0)==2)
                                    {
                                        h[n>>3]=p;
                                        f=q;
                                        break d
                                    }
                                    else
                                    {
                                        f=q;
                                        break d
                                    }
                                    else f=q;
                                    break
                                }
                            case 91:case 99:case 115:
                                {
                                    w=(s|0)==99;
                                    e:do if((s&239|0)==99)
                                    {
                                        Xn(J|0,-1,257)|0;
                                        a[J>>0]=0;
                                        if((s|0)==115)
                                        {
                                            a[F>>0]=0;
                                            a[E>>0]=0;
                                            a[E+1>>0]=0;
                                            a[E+2>>0]=0;
                                            a[E+3>>0]=0;
                                            a[E+4>>0]=0
                                        }
                                    }
                                    else
                                    {
                                        u=q+1|0;
                                        s=(a[u>>0]|0)==94;
                                        o=s&1;
                                        t=s?u:q;
                                        q=s?q+2|0:u;
                                        Xn(J|0,s&1|0,257)|0;
                                        a[J>>0]=0;
                                        s=a[q>>0]|0;
                                        if(s<<24>>24==93)
                                        {
                                            u=(o^1)&255;
                                            a[B>>0]=u;
                                            q=t+2|0
                                        }
                                        else if(s<<24>>24==45)
                                        {
                                            u=(o^1)&255;
                                            a[A>>0]=u;
                                            q=t+2|0
                                        }
                                        else u=(o^1)&255;
                                        while(1)
                                        {
                                            s=a[q>>0]|0;
                                            if(!(s<<24>>24))
                                            {
                                                O=164;
                                                break a
                                            }
                                            else if(s<<24>>24==45)
                                            {
                                                o=q+1|0;
                                                s=a[o>>0]|0;
                                                if(!(s<<24>>24==93|s<<24>>24==0))
                                                {
                                                    q=a[q+-1>>0]|0;
                                                    if((q&255)<(s&255))
                                                    {
                                                        q=q&255;
                                                        do
                                                        {
                                                            q=q+1|0;
                                                            a[J+q>>0]=u;
                                                            s=a[o>>0]|0
                                                        }
                                                        while((q|0)<(s&255|0));
                                                        q=o
                                                    }
                                                    else q=o
                                                }
                                                else s=45
                                            }
                                            else if(s<<24>>24==93)break e;
                                            a[J+((s&255)+1)>>0]=u;
                                            q=q+1|0
                                        }
                                    }
                                    while(0);
                                    o=w?x+1|0:31;
                                    t=(v|0)==1;
                                    u=(f|0)!=0;
                                    f:do if(t)
                                    {
                                        if(u)
                                        {
                                            l=$d(o<<2)|0;
                                            if(!l)
                                            {
                                                m=0;
                                                O=164;
                                                break a
                                            }
                                        }
                                        else l=n;
                                        c[K>>2]=0;
                                        c[H>>2]=0;
                                        m=0;
                                        g:while(1)
                                            {
                                            if(!l)
                                            {
                                                r=u&(m|0)==(o|0);
                                                while(1)
                                                {
                                                    s=c[M>>2]|0;
                                                    if(s>>>0<(c[N>>2]|0)>>>0)
                                                    {
                                                        c[M>>2]=s+1;
                                                        s=d[s>>0]|0
                                                    }
                                                    else s=Cd(e)|0;
                                                    if(!(a[J+(s+1)>>0]|0))
                                                    {
                                                        l=0;
                                                        break g
                                                    }
                                                    a[z>>0]=s;
                                                    s=qc(L,z,1,K)|0;
                                                    if((s|0)==-1)
                                                    {
                                                        m=0;
                                                        l=0;
                                                        O=164;
                                                        break a
                                                    }
                                                    else if((s|0)==-2)continue;
                                                    if(r)break
                                                }
                                            }
                                            else
                                            {
                                                if(!u)
                                                {
                                                    O=86;
                                                    break
                                                }
                                                while(1)
                                                {
                                                    while(1)
                                                    {
                                                        s=c[M>>2]|0;
                                                        if(s>>>0<(c[N>>2]|0)>>>0)
                                                        {
                                                            c[M>>2]=s+1;
                                                            s=d[s>>0]|0
                                                        }
                                                        else s=Cd(e)|0;
                                                        if(!(a[J+(s+1)>>0]|0))break g;
                                                        a[z>>0]=s;
                                                        s=qc(L,z,1,K)|0;
                                                        if((s|0)==-1)
                                                        {
                                                            m=0;
                                                            O=164;
                                                            break a
                                                        }
                                                        else if((s|0)!=-2)break
                                                    }
                                                    c[l+(m<<2)>>2]=c[L>>2];
                                                    m=m+1|0;
                                                    if((m|0)==(o|0))
                                                    {
                                                        m=o;
                                                        break
                                                    }
                                                }
                                            }
                                            s=o<<1|1;
                                            r=be(l,s<<2)|0;
                                            if(!r)
                                            {
                                                m=0;
                                                O=164;
                                                break a
                                            }
                                            o=s;
                                            l=r
                                        }
                                        h:do if((O|0)==86)
                                        {
                                            O=0;
                                            s=m;
                                            while(1)
                                            {
                                                while(1)
                                                {
                                                    m=c[M>>2]|0;
                                                    if(m>>>0<(c[N>>2]|0)>>>0)
                                                    {
                                                        c[M>>2]=m+1;
                                                        m=d[m>>0]|0
                                                    }
                                                    else m=Cd(e)|0;
                                                    if(!(a[J+(m+1)>>0]|0))
                                                    {
                                                        m=s;
                                                        break h
                                                    }
                                                    a[z>>0]=m;
                                                    m=qc(L,z,1,K)|0;
                                                    if((m|0)==-1)
                                                    {
                                                        f=0;
                                                        m=0;
                                                        O=164;
                                                        break a
                                                    }
                                                    else if((m|0)!=-2)break
                                                }
                                                c[l+(s<<2)>>2]=c[L>>2];
                                                s=s+1|0
                                            }
                                        }
                                        while(0);
                                        if(!(rc(K)|0))
                                        {
                                            m=0;
                                            O=164;
                                            break a
                                        }
                                        else
                                        {
                                            o=m;
                                            m=0
                                        }
                                    }
                                    else
                                    {
                                        if(u)
                                        {
                                            m=$d(o)|0;
                                            if(!m)
                                            {
                                                m=0;
                                                l=0;
                                                O=164;
                                                break a
                                            }
                                            else s=0;
                                            while(1)
                                            {
                                                do
                                                {
                                                    l=c[M>>2]|0;
                                                    if(l>>>0<(c[N>>2]|0)>>>0)
                                                    {
                                                        c[M>>2]=l+1;
                                                        l=d[l>>0]|0
                                                    }
                                                    else l=Cd(e)|0;
                                                    if(!(a[J+(l+1)>>0]|0))
                                                    {
                                                        o=s;
                                                        l=0;
                                                        break f
                                                    }
                                                    a[m+s>>0]=l;
                                                    s=s+1|0
                                                }
                                                while((s|0)!=(o|0));
                                                l=o<<1|1;
                                                s=be(m,l)|0;
                                                if(!s)
                                                {
                                                    l=0;
                                                    O=164;
                                                    break a
                                                }
                                                else
                                                {
                                                    v=o;
                                                    o=l;
                                                    m=s;
                                                    s=v
                                                }
                                            }
                                        }
                                        if(!n)
                                        {
                                            m=r;
                                            while(1)
                                            {
                                                l=c[M>>2]|0;
                                                if(l>>>0<m>>>0)
                                                {
                                                    c[M>>2]=l+1;
                                                    l=d[l>>0]|0
                                                }
                                                else l=Cd(e)|0;
                                                if(!(a[J+(l+1)>>0]|0))
                                                {
                                                    o=0;
                                                    m=0;
                                                    l=0;
                                                    break f
                                                }
                                                m=c[N>>2]|0
                                            }
                                        }
                                        else
                                        {
                                            m=0;
                                            while(1)
                                            {
                                                l=c[M>>2]|0;
                                                if(l>>>0<r>>>0)
                                                {
                                                    c[M>>2]=l+1;
                                                    l=d[l>>0]|0
                                                }
                                                else l=Cd(e)|0;
                                                if(!(a[J+(l+1)>>0]|0))
                                                {
                                                    o=m;
                                                    m=n;
                                                    l=0;
                                                    break f
                                                }
                                                a[n+m>>0]=l;
                                                r=c[N>>2]|0;
                                                m=m+1|0
                                            }
                                        }
                                    }
                                    while(0);
                                    r=c[M>>2]|0;
                                    if(c[N>>2]|0)
                                    {
                                        r=r+-1|0;
                                        c[M>>2]=r
                                    }
                                    s=r-(c[D>>2]|0)+(c[C>>2]|0)|0;
                                    if(!s)break a;
                                    if(!((s|0)==(x|0)|w^1))break a;
                                    do if(u)if(t)
                                    {
                                        c[n>>2]=l;
                                        break
                                    }
                                    else
                                    {
                                        c[n>>2]=m;
                                        break
                                    }
                                    while(0);
                                    if(!w)
                                    {
                                        if(l)c[l+(o<<2)>>2]=0;
                                        if(!m)
                                        {
                                            f=q;
                                            m=0
                                        }
                                        else
                                        {
                                            a[m+o>>0]=0;
                                            f=q
                                        }
                                    }
                                    else f=q;
                                    break
                                }
                            default:f=q
                        }
                        while(0);
                        i:do if((O|0)==146)
                        {
                            O=0;
                            o=zd(e,o,0,-1,-1)|0;
                            if((c[C>>2]|0)==((c[D>>2]|0)-(c[M>>2]|0)|0))break a;
                            if((n|0)!=0&(s|0)==112)
                            {
                                c[n>>2]=o;
                                f=q;
                                break
                            }
                            if(!n)f=q;
                            else switch(v|0)
                            {
                                case 1:
                                    {
                                        c[n>>2]=o;
                                        f=q;
                                        break i
                                    }
                                case -2:
                                    {
                                        a[n>>0]=o;
                                        f=q;
                                        break i
                                    }
                                case 0:
                                    {
                                        c[n>>2]=o;
                                        f=q;
                                        break i
                                    }
                                case -1:
                                    {
                                        b[n>>1]=o;
                                        f=q;
                                        break i
                                    }
                                case 3:
                                    {
                                        f=n;
                                        c[f>>2]=o;
                                        c[f+4>>2]=G;
                                        f=q;
                                        break i
                                    }
                                default:
                                    {
                                        f=q;
                                        break i
                                    }
                            }
                        }
                        while(0);
                        k=((n|0)!=0&1)+k|0;
                        s=(c[C>>2]|0)+y+(c[M>>2]|0)-(c[D>>2]|0)|0;
                        break b
                    }
                    while(0);
                    o=f+(o&1)|0;
                    Bd(e,0);
                    f=c[M>>2]|0;
                    if(f>>>0<(c[N>>2]|0)>>>0)
                    {
                        c[M>>2]=f+1;
                        f=d[f>>0]|0
                    }
                    else f=Cd(e)|0;
                    if((f|0)!=(d[o>>0]|0))
                    {
                        O=19;
                        break a
                    }
                    f=o;
                    s=t+1|0
                }
                else
                {
                    while(1)
                    {
                        n=f+1|0;
                        if(!(xd(d[n>>0]|0)|0))break;
                        else f=n
                    }
                    Bd(e,0);
                    do
                    {
                        n=c[M>>2]|0;
                        if(n>>>0<(c[N>>2]|0)>>>0)
                        {
                            c[M>>2]=n+1;
                            n=d[n>>0]|0
                        }
                        else n=Cd(e)|0
                    }
                    while((xd(n)|0)!=0);
                    n=c[M>>2]|0;
                    if(c[N>>2]|0)
                    {
                        n=n+-1|0;
                        c[M>>2]=n
                    }
                    s=(c[C>>2]|0)+t+n-(c[D>>2]|0)|0
                }
                while(0);
            f=f+1|0;
            q=a[f>>0]|0;
            if(!(q<<24>>24))
            {
                O=168;
                break
            }
            else t=s
        }
        if((O|0)==19)
        {
            if(c[N>>2]|0)c[M>>2]=(c[M>>2]|0)+-1;
            if((k|0)!=0|(f|0)>-1)
            {
                O=k;
                i=P;
                return O|0
            }
            else
            {
                k=0;
                O=165
            }
        }
        else if((O|0)==164)
        {
            if(!k)
            {
                k=f;
                O=165
            }
        }
        else if((O|0)==168)
        {
            i=P;
            return k|0
        }
        if((O|0)==165)
        {
            f=k;
            k=-1
        }
        if(!f)
        {
            O=k;
            i=P;
            return O|0
        }
        ae(m);
        ae(l);
        O=k;
        i=P;
        return O|0
    }
    function Bc(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0;
        g=i;
        i=i+112|0;
        e=g;
        f=e;
        h=f+112|0;
        do
        {
            c[f>>2]=0;
            f=f+4|0
        }
        while((f|0)<(h|0));
        c[e+32>>2]=22;
        c[e+44>>2]=a;
        c[e+76>>2]=-1;
        c[e+84>>2]=a;
        h=Ac(e,b,d)|0;
        i=g;
        return h|0
    }
    function Cc(a)
    {
        a=a|0;
        var b=0;
        b=a;
        while(1)if(!(c[b>>2]|0))break;
        else b=b+4|0;
        return b-a>>2|0
    }
    function Dc(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0;
        if(!d)return a|0;
        else e=a;
        while(1)
        {
            d=d+-1|0;
            c[e>>2]=c[b>>2];
            if(!d)break;
            else
            {
                b=b+4|0;
                e=e+4|0
            }
        }
        return a|0
    }
    function Ec(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0;
        e=(d|0)==0;
        if(a-b>>2>>>0<d>>>0)
        {
            if(e)return a|0;
            do
            {
                d=d+-1|0;
                c[a+(d<<2)>>2]=c[b+(d<<2)>>2]
            }
            while((d|0)!=0);
            return a|0
        }
        else
        {
            if(e)return a|0;
            else
            {
                e=b;
                b=a
            }
            while(1)
            {
                d=d+-1|0;
                c[b>>2]=c[e>>2];
                if(!d)break;
                else
                {
                    e=e+4|0;
                    b=b+4|0
                }
            }
            return a|0
        }
        return 0
    }
    function Fc(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0;
        if(!d)return a|0;
        else e=a;
        while(1)
        {
            d=d+-1|0;
            c[e>>2]=b;
            if(!d)break;
            else e=e+4|0
        }
        return a|0
    }
    function Gc(a,b,c)
    {
        a=a|0;
        b=b|0;
        c=c|0;
        return xc(a,b,c)|0
    }
    function Hc(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0;
        d=i;
        i=i+16|0;
        c[d>>2]=b;
        b=c[o>>2]|0;
        Ga(b|0,a|0,d|0)|0;
        tb(10,b|0)|0;
        xb()
    }
    function Ic(a)
    {
        a=a|0;
        Ia(824,848,303,928)
    }
    function Jc()
    {
        var a=0,b=0;
        a=i;
        i=i+16|0;
        if(!(hb(960,2)|0))
        {
            b=cb(c[238]|0)|0;
            i=a;
            return b|0
        }
        else Hc(968,a);
        return 0
    }
    function Kc(a)
    {
        a=a|0;
        var b=0;
        b=(a|0)==0?1:a;
        a=$d(b)|0;
        a:do if(!a)
        {
            while(1)
            {
                a=Tc()|0;
                if(!a)break;
                Sb[a&3]();
                a=$d(b)|0;
                if(a)break a
            }
            b=Ja(4)|0;
            c[b>>2]=264;
            ub(b|0,296,1)
        }
        while(0);
        return a|0
    }
    function Lc(a)
    {
        a=a|0;
        ae(a);
        return
    }
    function Mc(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Nc(a)
    {
        a=a|0;
        c[a>>2]=264;
        return
    }
    function Oc(a)
    {
        a=a|0;
        return
    }
    function Pc(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function Qc(a)
    {
        a=a|0;
        return 1024
    }
    function Rc(a)
    {
        a=a|0;
        var b=0;
        b=i;
        i=i+16|0;
        Sb[a&3]();
        Hc(1040,b)
    }
    function Sc()
    {
        var a=0,b=0;
        a=Jc()|0;
        if(((a|0)!=0?(b=c[a>>2]|0,(b|0)!=0):0)?(a=b+48|0,(c[a>>2]&-256|0)==1126902528?(c[a+4>>2]|0)==1129074247:0):0)Rc(c[b+12>>2]|0);
        b=c[62]|0;
        c[62]=b+0;
        Rc(b)
    }
    function Tc()
    {
        var a=0;
        a=c[78]|0;
        c[78]=a+0;
        return a|0
    }
    function Uc(a)
    {
        a=a|0;
        return
    }
    function Vc(a)
    {
        a=a|0;
        return
    }
    function Wc(a)
    {
        a=a|0;
        return
    }
    function Xc(a)
    {
        a=a|0;
        return
    }
    function Yc(a)
    {
        a=a|0;
        return
    }
    function Zc(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function _c(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function $c(a)
    {
        a=a|0;
        Lc(a);
        return
    }
    function ad(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0;
        h=i;
        i=i+64|0;
        g=h;
        if((a|0)!=(b|0))if((b|0)!=0?(f=gd(b,408,464,0)|0,(f|0)!=0):0)
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
            Yb[c[(c[f>>2]|0)+28>>2]&7](f,g,c[d>>2]|0,1);
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
    function bd(b,d,e,f)
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
    function cd(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        if((a|0)==(c[b+8>>2]|0))bd(0,b,d,e);
        return
    }
    function dd(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        if((a|0)==(c[b+8>>2]|0))bd(0,b,d,e);
        else
        {
            a=c[a+8>>2]|0;
            Yb[c[(c[a>>2]|0)+28>>2]&7](a,b,d,e)
        }
        return
    }
    function ed(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        g=c[a+4>>2]|0;
        f=g>>8;
        if(g&1)f=c[(c[d>>2]|0)+f>>2]|0;
        a=c[a>>2]|0;
        Yb[c[(c[a>>2]|0)+28>>2]&7](a,b,d+f|0,(g&2|0)!=0?e:2);
        return
    }
    function fd(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0;
        a:do if((b|0)!=(c[d+8>>2]|0))
        {
            h=c[b+12>>2]|0;
            g=b+16+(h<<3)|0;
            ed(b+16|0,d,e,f);
            if((h|0)>1)
            {
                h=d+54|0;
                b=b+24|0;
                do
                {
                    ed(b,d,e,f);
                    if(a[h>>0]|0)break a;
                    b=b+8|0
                }
                while(b>>>0<g>>>0)
            }
        }
        else bd(0,d,e,f);
        while(0);
        return
    }
    function gd(d,e,f,g)
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
            Vb[c[(c[f>>2]|0)+20>>2]&7](f,q,o,o,1,0);
            g=(c[k>>2]|0)==1?o:0
        }
        else
        {
            Kb[c[(c[p>>2]|0)+24>>2]&3](p,q,o,1,0);
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
    function hd(b,d,e,f,g)
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
    function id(b,d,e,f,g)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;
        a:do if((b|0)==(c[d+8>>2]|0))
        {
            if((c[d+4>>2]|0)==(e|0)?(h=d+28|0,(c[h>>2]|0)!=1):0)c[h>>2]=f
        }
        else
        {
            if((b|0)!=(c[d>>2]|0))
            {
                r=c[b+12>>2]|0;
                k=b+16+(r<<3)|0;
                kd(b+16|0,d,e,f,g);
                h=b+24|0;
                if((r|0)<=1)break;
                i=c[b+8>>2]|0;
                if((i&2|0)==0?(l=d+36|0,(c[l>>2]|0)!=1):0)
                {
                    if(!(i&1))
                    {
                        i=d+54|0;
                        while(1)
                        {
                            if(a[i>>0]|0)break a;
                            if((c[l>>2]|0)==1)break a;
                            kd(h,d,e,f,g);
                            h=h+8|0;
                            if(h>>>0>=k>>>0)break a
                        }
                    }
                    i=d+24|0;
                    j=d+54|0;
                    while(1)
                    {
                        if(a[j>>0]|0)break a;
                        if((c[l>>2]|0)==1?(c[i>>2]|0)==1:0)break a;
                        kd(h,d,e,f,g);
                        h=h+8|0;
                        if(h>>>0>=k>>>0)break a
                    }
                }
                i=d+54|0;
                while(1)
                {
                    if(a[i>>0]|0)break a;
                    kd(h,d,e,f,g);
                    h=h+8|0;
                    if(h>>>0>=k>>>0)break a
                }
            }
            if((c[d+16>>2]|0)!=(e|0)?(q=d+20|0,(c[q>>2]|0)!=(e|0)):0)
            {
                c[d+32>>2]=f;
                n=d+44|0;
                if((c[n>>2]|0)==4)break;
                i=c[b+12>>2]|0;
                j=b+16+(i<<3)|0;
                l=d+52|0;
                f=d+53|0;
                o=d+54|0;
                m=b+8|0;
                p=d+24|0;
                b:do if((i|0)>0)
                {
                    i=0;
                    h=0;
                    k=b+16|0;
                    while(1)
                    {
                        a[l>>0]=0;
                        a[f>>0]=0;
                        jd(k,d,e,e,1,g);
                        if(a[o>>0]|0)
                        {
                            r=20;
                            break b
                        }
                        do if(a[f>>0]|0)
                        {
                            if(!(a[l>>0]|0))if(!(c[m>>2]&1))
                            {
                                h=1;
                                r=20;
                                break b
                            }
                            else
                            {
                                h=1;
                                break
                            }
                            if((c[p>>2]|0)==1)break b;
                            if(!(c[m>>2]&2))break b;
                            else
                            {
                                i=1;
                                h=1
                            }
                        }
                        while(0);
                        k=k+8|0;
                        if(k>>>0>=j>>>0)
                        {
                            r=20;
                            break
                        }
                    }
                }
                else
                {
                    i=0;
                    h=0;
                    r=20
                }
                while(0);
                do if((r|0)==20)
                {
                    if((!i?(c[q>>2]=e,e=d+40|0,c[e>>2]=(c[e>>2]|0)+1,(c[d+36>>2]|0)==1):0)?(c[p>>2]|0)==2:0)
                    {
                        a[o>>0]=1;
                        if(h)break
                    }
                    else r=24;
                    if((r|0)==24?h:0)break;
                    c[n>>2]=4;
                    break a
                }
                while(0);
                c[n>>2]=3;
                break
            }
            if((f|0)==1)c[d+32>>2]=1
        }
        while(0);
        return
    }
    function jd(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        var h=0,i=0;
        i=c[a+4>>2]|0;
        h=i>>8;
        if(i&1)h=c[(c[e>>2]|0)+h>>2]|0;
        a=c[a>>2]|0;
        Vb[c[(c[a>>2]|0)+20>>2]&7](a,b,d,e+h|0,(i&2|0)!=0?f:2,g);
        return
    }
    function kd(a,b,d,e,f)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0;
        h=c[a+4>>2]|0;
        g=h>>8;
        if(h&1)g=c[(c[d>>2]|0)+g>>2]|0;
        a=c[a>>2]|0;
        Kb[c[(c[a>>2]|0)+24>>2]&3](a,b,d+g|0,(h&2|0)!=0?e:2,f);
        return
    }
    function ld(b,d,e,f,g)
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
                Kb[c[(c[i>>2]|0)+24>>2]&3](i,d,e,f,g);
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
                Vb[c[(c[b>>2]|0)+20>>2]&7](b,d,e,e,1,g);
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
    function md(b,d,e,f,g)
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
    function nd(b,d,e,f,g,h)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        if((b|0)==(c[d+8>>2]|0))hd(0,d,e,f,g);
        else
        {
            m=d+52|0;
            n=a[m>>0]|0;
            o=d+53|0;
            p=a[o>>0]|0;
            l=c[b+12>>2]|0;
            i=b+16+(l<<3)|0;
            a[m>>0]=0;
            a[o>>0]=0;
            jd(b+16|0,d,e,f,g,h);
            a:do if((l|0)>1)
            {
                j=d+24|0;
                k=b+8|0;
                l=d+54|0;
                b=b+24|0;
                do
                {
                    if(a[l>>0]|0)break a;
                    if(!(a[m>>0]|0))
                    {
                        if((a[o>>0]|0)!=0?(c[k>>2]&1|0)==0:0)break a
                    }
                    else
                    {
                        if((c[j>>2]|0)==1)break a;
                        if(!(c[k>>2]&2))break a
                    }
                    a[m>>0]=0;
                    a[o>>0]=0;
                    jd(b,d,e,f,g,h);
                    b=b+8|0
                }
                while(b>>>0<i>>>0)
            }
            while(0);
            a[m>>0]=n;
            a[o>>0]=p
        }
        return
    }
    function od(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        if((a|0)==(c[b+8>>2]|0))hd(0,b,d,e,f);
        else
        {
            a=c[a+8>>2]|0;
            Vb[c[(c[a>>2]|0)+20>>2]&7](a,b,d,e,f,g)
        }
        return
    }
    function pd(a,b,d,e,f,g)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        g=g|0;
        if((a|0)==(c[b+8>>2]|0))hd(0,b,d,e,f);
        return
    }
    function qd(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0;
        f=i;
        i=i+16|0;
        e=f;
        c[e>>2]=c[d>>2];
        a=Jb[c[(c[a>>2]|0)+16>>2]&31](a,b,e)|0;
        if(a)c[d>>2]=c[e>>2];
        i=f;
        return a&1|0
    }
    function rd(a)
    {
        a=a|0;
        if(!a)a=0;
        else a=(gd(a,408,576,0)|0)!=0;
        return a&1|0
    }
    function sd()
    {
        var a=0;
        a=Ja(4)|0;
        Nc(a);
        ub(a|0,296,1)
    }
    function td()
    {
        var a=0,b=0,d=0,e=0,f=0,g=0,h=0,j=0;
        f=i;
        i=i+48|0;
        h=f+32|0;
        d=f;
        j=f+8|0;
        g=f+16|0;
        f=f+36|0;
        a=Jc()|0;
        if((a|0)!=0?(e=c[a>>2]|0,(e|0)!=0):0)
        {
            a=e+48|0;
            b=c[a>>2]|0;
            a=c[a+4>>2]|0;
            if(!((b&-256|0)==1126902528&(a|0)==1129074247))
            {
                c[d>>2]=c[326];
                Hc(1408,d)
            }
            if((b|0)==1126902529&(a|0)==1129074247)a=c[e+44>>2]|0;
            else a=e+80|0;
            c[f>>2]=a;
            d=c[e>>2]|0;
            a=c[d+4>>2]|0;
            if(Jb[c[(c[336>>2]|0)+16>>2]&31](336,d,f)|0)
            {
                f=c[f>>2]|0;
                d=c[326]|0;
                f=Qb[c[(c[f>>2]|0)+8>>2]&63](f)|0;
                c[g>>2]=d;
                c[g+4>>2]=a;
                c[g+8>>2]=f;
                Hc(1312,g)
            }
            else
            {
                c[j>>2]=c[326];
                c[j+4>>2]=a;
                Hc(1360,j)
            }
        }
        Hc(1448,h)
    }
    function ud()
    {
        var a=0;
        a=i;
        i=i+16|0;
        if(!(kb(952,94)|0))
        {
            i=a;
            return
        }
        else Hc(1080,a)
    }
    function vd(a)
    {
        a=a|0;
        var b=0;
        b=i;
        i=i+16|0;
        ae(a);
        if(!(qb(c[238]|0,0)|0))
        {
            i=b;
            return
        }
        else Hc(1136,b)
    }
    function wd(a)
    {
        a=a|0;
        Ia(1192,1224,1164,928)
    }
    function xd(a)
    {
        a=a|0;
        return ((a|0)==32|(a+-9|0)>>>0<5)&1|0
    }
    function yd(a)
    {
        a=a|0;
        if((a+-48|0)>>>0<10)
        {
            a=1;
            a=a&1;
            return a|0
        }
        a=((a|32)+-97|0)>>>0<6;
        a=a&1;
        return a|0
    }
    function zd(b,e,f,g,h)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        g=g|0;
        h=h|0;
        var i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;
        if(e>>>0>36)
        {
            c[(ob()|0)>>2]=22;
            q=0;
            r=0;
            G=q;
            return r|0
        }
        r=b+4|0;
        q=b+100|0;
        do
        {
            i=c[r>>2]|0;
            if(i>>>0<(c[q>>2]|0)>>>0)
            {
                c[r>>2]=i+1;
                i=d[i>>0]|0
            }
            else i=Cd(b)|0
        }
        while((xd(i)|0)!=0);
        do if((i|0)==43|(i|0)==45)
        {
            k=((i|0)==45)<<31>>31;
            i=c[r>>2]|0;
            if(i>>>0<(c[q>>2]|0)>>>0)
            {
                c[r>>2]=i+1;
                i=d[i>>0]|0;
                p=k;
                break
            }
            else
            {
                i=Cd(b)|0;
                p=k;
                break
            }
        }
        else p=0;
        while(0);
        j=(e|0)==0;
        do if((e&-17|0)==0&(i|0)==48)
        {
            k=c[r>>2]|0;
            if(k>>>0<(c[q>>2]|0)>>>0)
            {
                c[r>>2]=k+1;
                i=d[k>>0]|0
            }
            else i=Cd(b)|0;
            if((i|32|0)!=120)if(j)
            {
                e=8;
                n=46;
                break
            }
            else
            {
                n=32;
                break
            }
            e=c[r>>2]|0;
            if(e>>>0<(c[q>>2]|0)>>>0)
            {
                c[r>>2]=e+1;
                i=d[e>>0]|0
            }
            else i=Cd(b)|0;
            if((d[1480+(i+1)>>0]|0)>15)
            {
                e=(c[q>>2]|0)==0;
                if(!e)c[r>>2]=(c[r>>2]|0)+-1;
                if(!f)
                {
                    Bd(b,0);
                    q=0;
                    r=0;
                    G=q;
                    return r|0
                }
                if(e)
                {
                    q=0;
                    r=0;
                    G=q;
                    return r|0
                }
                c[r>>2]=(c[r>>2]|0)+-1;
                q=0;
                r=0;
                G=q;
                return r|0
            }
            else
            {
                e=16;
                n=46
            }
        }
        else
        {
            e=j?10:e;
            if((d[1480+(i+1)>>0]|0)>>>0<e>>>0)n=32;
            else
            {
                if(c[q>>2]|0)c[r>>2]=(c[r>>2]|0)+-1;
                Bd(b,0);
                c[(ob()|0)>>2]=22;
                q=0;
                r=0;
                G=q;
                return r|0
            }
        }
        while(0);
        if((n|0)==32)if((e|0)==10)
        {
            e=i+-48|0;
            if(e>>>0<10)
            {
                k=e;
                e=0;
                do
                {
                    e=(e*10|0)+k|0;
                    i=c[r>>2]|0;
                    if(i>>>0<(c[q>>2]|0)>>>0)
                    {
                        c[r>>2]=i+1;
                        i=d[i>>0]|0
                    }
                    else i=Cd(b)|0;
                    k=i+-48|0
                }
                while(k>>>0<10&e>>>0<429496729);
                k=0
            }
            else
            {
                e=0;
                k=0
            }
            j=i+-48|0;
            if(j>>>0<10)
            {
                while(1)
                {
                    l=ho(e|0,k|0,10,0)|0;
                    f=G;
                    m=((j|0)<0)<<31>>31;
                    o=~m;
                    if(f>>>0>o>>>0|(f|0)==(o|0)&l>>>0>~j>>>0)
                    {
                        l=e;
                        break
                    }
                    e=Wn(l|0,f|0,j|0,m|0)|0;
                    k=G;
                    i=c[r>>2]|0;
                    if(i>>>0<(c[q>>2]|0)>>>0)
                    {
                        c[r>>2]=i+1;
                        i=d[i>>0]|0
                    }
                    else i=Cd(b)|0;
                    j=i+-48|0;
                    if(!(j>>>0<10&(k>>>0<429496729|(k|0)==429496729&e>>>0<2576980378)))
                    {
                        l=e;
                        break
                    }
                }
                if(j>>>0>9)e=l;
                else
                {
                    e=10;
                    n=72
                }
            }
        }
        else n=46;
        a:do if((n|0)==46)
        {
            if(!(e+-1&e))
            {
                n=a[1744+((e*23|0)>>>5&7)>>0]|0;
                k=a[1480+(i+1)>>0]|0;
                j=k&255;
                if(j>>>0<e>>>0)
                {
                    k=0;
                    while(1)
                    {
                        l=j|k<<n;
                        k=c[r>>2]|0;
                        if(k>>>0<(c[q>>2]|0)>>>0)
                        {
                            c[r>>2]=k+1;
                            i=d[k>>0]|0
                        }
                        else i=Cd(b)|0;
                        k=a[1480+(i+1)>>0]|0;
                        j=k&255;
                        if(!(l>>>0<134217728&j>>>0<e>>>0))break;
                        else k=l
                    }
                    j=0
                }
                else
                {
                    j=0;
                    l=0
                }
                f=Zn(-1,-1,n|0)|0;
                m=G;
                if((k&255)>>>0>=e>>>0|(j>>>0>m>>>0|(j|0)==(m|0)&l>>>0>f>>>0))
                {
                    k=j;
                    n=72;
                    break
                }
                else i=j;
                while(1)
                {
                    l=$n(l|0,i|0,n|0)|0;
                    j=G;
                    l=k&255|l;
                    k=c[r>>2]|0;
                    if(k>>>0<(c[q>>2]|0)>>>0)
                    {
                        c[r>>2]=k+1;
                        i=d[k>>0]|0
                    }
                    else i=Cd(b)|0;
                    k=a[1480+(i+1)>>0]|0;
                    if((k&255)>>>0>=e>>>0|(j>>>0>m>>>0|(j|0)==(m|0)&l>>>0>f>>>0))
                    {
                        k=j;
                        n=72;
                        break a
                    }
                    else i=j
                }
            }
            j=a[1480+(i+1)>>0]|0;
            k=j&255;
            if(k>>>0<e>>>0)
            {
                i=0;
                while(1)
                {
                    l=k+(ca(i,e)|0)|0;
                    i=c[r>>2]|0;
                    if(i>>>0<(c[q>>2]|0)>>>0)
                    {
                        c[r>>2]=i+1;
                        i=d[i>>0]|0
                    }
                    else i=Cd(b)|0;
                    j=a[1480+(i+1)>>0]|0;
                    k=j&255;
                    if(!(l>>>0<119304647&k>>>0<e>>>0))break;
                    else i=l
                }
                k=0
            }
            else
            {
                l=0;
                k=0
            }
            if((j&255)>>>0<e>>>0)
            {
                n=io(-1,-1,e|0,0)|0;
                o=G;
                while(1)
                {
                    if(k>>>0>o>>>0|(k|0)==(o|0)&l>>>0>n>>>0)
                    {
                        n=72;
                        break a
                    }
                    f=ho(l|0,k|0,e|0,0)|0;
                    m=G;
                    j=j&255;
                    if(m>>>0>4294967295|(m|0)==-1&f>>>0>~j>>>0)
                    {
                        n=72;
                        break a
                    }
                    l=Wn(j|0,0,f|0,m|0)|0;
                    k=G;
                    i=c[r>>2]|0;
                    if(i>>>0<(c[q>>2]|0)>>>0)
                    {
                        c[r>>2]=i+1;
                        i=d[i>>0]|0
                    }
                    else i=Cd(b)|0;
                    j=a[1480+(i+1)>>0]|0;
                    if((j&255)>>>0>=e>>>0)
                    {
                        n=72;
                        break
                    }
                }
            }
            else n=72
        }
        while(0);
        if((n|0)==72)if((d[1480+(i+1)>>0]|0)>>>0<e>>>0)
        {
            do
            {
                i=c[r>>2]|0;
                if(i>>>0<(c[q>>2]|0)>>>0)
                {
                    c[r>>2]=i+1;
                    i=d[i>>0]|0
                }
                else i=Cd(b)|0
            }
            while((d[1480+(i+1)>>0]|0)>>>0<e>>>0);
            c[(ob()|0)>>2]=34;
            k=h;
            e=g
        }
        else e=l;
        if(c[q>>2]|0)c[r>>2]=(c[r>>2]|0)+-1;
        if(!(k>>>0<h>>>0|(k|0)==(h|0)&e>>>0<g>>>0))
        {
            if(!((g&1|0)!=0|0!=0|(p|0)!=0))
            {
                c[(ob()|0)>>2]=34;
                r=Wn(g|0,h|0,-1,-1)|0;
                q=G;
                G=q;
                return r|0
            }
            if(k>>>0>h>>>0|(k|0)==(h|0)&e>>>0>g>>>0)
            {
                c[(ob()|0)>>2]=34;
                q=h;
                r=g;
                G=q;
                return r|0
            }
        }
        r=((p|0)<0)<<31>>31;
        r=Vn(e^p|0,k^r|0,p|0,r|0)|0;
        q=G;
        G=q;
        return r|0
    }
    function Ad(b,e,f)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        var g=0.0,h=0,j=0.0,k=0,l=0,m=0.0,n=0,o=0,p=0,q=0,r=0.0,s=0.0,t=0,u=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,H=0,I=0,J=0,K=0,L=0,M=0.0;
        L=i;
        i=i+512|0;
        H=L;
        if((e|0)==1)
        {
            K=53;
            I=-1074
        }
        else if((e|0)==2)
        {
            K=53;
            I=-1074
        }
        else if(!e)
        {
            K=24;
            I=-149
        }
        else
        {
            s=0.0;
            i=L;
            return +s
        }
        D=b+4|0;
        C=b+100|0;
        do
        {
            e=c[D>>2]|0;
            if(e>>>0<(c[C>>2]|0)>>>0)
            {
                c[D>>2]=e+1;
                e=d[e>>0]|0
            }
            else e=Cd(b)|0
        }
        while((xd(e)|0)!=0);
        do if((e|0)==43|(e|0)==45)
        {
            h=1-(((e|0)==45&1)<<1)|0;
            e=c[D>>2]|0;
            if(e>>>0<(c[C>>2]|0)>>>0)
            {
                c[D>>2]=e+1;
                e=d[e>>0]|0;
                J=h;
                break
            }
            else
            {
                e=Cd(b)|0;
                J=h;
                break
            }
        }
        else J=1;
        while(0);
        h=0;
        do
        {
            if((e|32|0)!=(a[1760+h>>0]|0))break;
            do if(h>>>0<7)
            {
                e=c[D>>2]|0;
                if(e>>>0<(c[C>>2]|0)>>>0)
                {
                    c[D>>2]=e+1;
                    e=d[e>>0]|0;
                    break
                }
                else
                {
                    e=Cd(b)|0;
                    break
                }
            }
            while(0);
            h=h+1|0
        }
        while(h>>>0<8);
        do if((h|0)==3)A=23;
        else if((h|0)!=8)
        {
            n=(f|0)!=0;
            if(n&h>>>0>3)if((h|0)==8)break;
            else
            {
                A=23;
                break
            }
            do if(!h)
            {
                if((e|32|0)==110)
                {
                    e=c[D>>2]|0;
                    if(e>>>0<(c[C>>2]|0)>>>0)
                    {
                        c[D>>2]=e+1;
                        e=d[e>>0]|0
                    }
                    else e=Cd(b)|0;
                    if((e|32|0)!=97)break;
                    e=c[D>>2]|0;
                    if(e>>>0<(c[C>>2]|0)>>>0)
                    {
                        c[D>>2]=e+1;
                        e=d[e>>0]|0
                    }
                    else e=Cd(b)|0;
                    if((e|32|0)!=110)break;
                    e=c[D>>2]|0;
                    if(e>>>0<(c[C>>2]|0)>>>0)
                    {
                        c[D>>2]=e+1;
                        e=d[e>>0]|0
                    }
                    else e=Cd(b)|0;
                    if((e|0)==40)h=1;
                    else
                    {
                        if(!(c[C>>2]|0))
                        {
                            s=v;
                            i=L;
                            return +s
                        }
                        c[D>>2]=(c[D>>2]|0)+-1;
                        s=v;
                        i=L;
                        return +s
                    }
                    while(1)
                    {
                        e=c[D>>2]|0;
                        if(e>>>0<(c[C>>2]|0)>>>0)
                        {
                            c[D>>2]=e+1;
                            e=d[e>>0]|0
                        }
                        else e=Cd(b)|0;
                        if(!((e+-48|0)>>>0<10|(e+-65|0)>>>0<26)?!((e|0)==95|(e+-97|0)>>>0<26):0)break;
                        h=h+1|0
                    }
                    if((e|0)==41)
                    {
                        s=v;
                        i=L;
                        return +s
                    }
                    e=(c[C>>2]|0)==0;
                    if(!e)c[D>>2]=(c[D>>2]|0)+-1;
                    if(!n)
                    {
                        c[(ob()|0)>>2]=22;
                        Bd(b,0);
                        s=0.0;
                        i=L;
                        return +s
                    }
                    if((h|0)==0|e)
                    {
                        s=v;
                        i=L;
                        return +s
                    }
                    e=c[D>>2]|0;
                    do
                    {
                        h=h+-1|0;
                        e=e+-1|0
                    }
                    while((h|0)!=0);
                    c[D>>2]=e;
                    s=v;
                    i=L;
                    return +s
                }
                do if((e|0)==48)
                {
                    e=c[D>>2]|0;
                    if(e>>>0<(c[C>>2]|0)>>>0)
                    {
                        c[D>>2]=e+1;
                        e=d[e>>0]|0
                    }
                    else e=Cd(b)|0;
                    if((e|32|0)!=120)
                    {
                        if(!(c[C>>2]|0))
                        {
                            e=48;
                            break
                        }
                        c[D>>2]=(c[D>>2]|0)+-1;
                        e=48;
                        break
                    }
                    e=c[D>>2]|0;
                    if(e>>>0<(c[C>>2]|0)>>>0)
                    {
                        c[D>>2]=e+1;
                        n=d[e>>0]|0;
                        o=0
                    }
                    else
                    {
                        n=Cd(b)|0;
                        o=0
                    }
                    while(1)
                    {
                        if((n|0)==46)
                        {
                            A=71;
                            break
                        }
                        else if((n|0)!=48)
                        {
                            e=0;
                            y=0;
                            l=0;
                            h=0;
                            p=0;
                            x=0;
                            m=1.0;
                            k=0;
                            g=0.0;
                            break
                        }
                        e=c[D>>2]|0;
                        if(e>>>0<(c[C>>2]|0)>>>0)
                        {
                            c[D>>2]=e+1;
                            n=d[e>>0]|0;
                            o=1;
                            continue
                        }
                        else
                        {
                            n=Cd(b)|0;
                            o=1;
                            continue
                        }
                    }
                    if((A|0)==71)
                    {
                        e=c[D>>2]|0;
                        if(e>>>0<(c[C>>2]|0)>>>0)
                        {
                            c[D>>2]=e+1;
                            n=d[e>>0]|0
                        }
                        else n=Cd(b)|0;
                        if((n|0)==48)
                        {
                            l=0;
                            h=0;
                            do
                            {
                                e=c[D>>2]|0;
                                if(e>>>0<(c[C>>2]|0)>>>0)
                                {
                                    c[D>>2]=e+1;
                                    n=d[e>>0]|0
                                }
                                else n=Cd(b)|0;
                                l=Wn(l|0,h|0,-1,-1)|0;
                                h=G
                            }
                            while((n|0)==48);
                            e=0;
                            y=0;
                            o=1;
                            p=1;
                            x=0;
                            m=1.0;
                            k=0;
                            g=0.0
                        }
                        else
                        {
                            e=0;
                            y=0;
                            l=0;
                            h=0;
                            p=1;
                            x=0;
                            m=1.0;
                            k=0;
                            g=0.0
                        }
                    }
                    while(1)
                    {
                        t=n+-48|0;
                        q=n|32;
                        if(t>>>0>=10)
                        {
                            u=(n|0)==46;
                            if(!(u|(q+-97|0)>>>0<6))
                            {
                                q=y;
                                break
                            }
                            if(u)if(!p)
                            {
                                l=y;
                                h=e;
                                t=y;
                                p=1;
                                q=x;
                                j=m
                            }
                            else
                            {
                                q=y;
                                n=46;
                                break
                            }
                            else A=83
                        }
                        else A=83;
                        if((A|0)==83)
                        {
                            A=0;
                            n=(n|0)>57?q+-87|0:t;
                            do if(!((e|0)<0|(e|0)==0&y>>>0<8))
                            {
                                if((e|0)<0|(e|0)==0&y>>>0<14)
                                {
                                    s=m*.0625;
                                    q=x;
                                    j=s;
                                    g=g+s*+(n|0);
                                    break
                                }
                                if((x|0)!=0|(n|0)==0)
                                {
                                    q=x;
                                    j=m
                                }
                                else
                                {
                                    q=1;
                                    j=m;
                                    g=g+m*.5
                                }
                            }
                            else
                            {
                                q=x;
                                j=m;
                                k=n+(k<<4)|0
                            }
                            while(0);
                            t=Wn(y|0,e|0,1,0)|0;
                            e=G;
                            o=1
                        }
                        n=c[D>>2]|0;
                        if(n>>>0<(c[C>>2]|0)>>>0)
                        {
                            c[D>>2]=n+1;
                            y=t;
                            n=d[n>>0]|0;
                            x=q;
                            m=j;
                            continue
                        }
                        else
                        {
                            y=t;
                            n=Cd(b)|0;
                            x=q;
                            m=j;
                            continue
                        }
                    }
                    if(!o)
                    {
                        e=(c[C>>2]|0)==0;
                        if(!e)c[D>>2]=(c[D>>2]|0)+-1;
                        if(f)
                        {
                            if(!e?(z=c[D>>2]|0,c[D>>2]=z+-1,(p|0)!=0):0)c[D>>2]=z+-2
                        }
                        else Bd(b,0);
                        s=+(J|0)*0.0;
                        i=L;
                        return +s
                    }
                    p=(p|0)==0;
                    o=p?q:l;
                    p=p?e:h;
                    if((e|0)<0|(e|0)==0&q>>>0<8)
                    {
                        h=q;
                        do
                        {
                            k=k<<4;
                            h=Wn(h|0,e|0,1,0)|0;
                            e=G
                        }
                        while((e|0)<0|(e|0)==0&h>>>0<8)
                    }
                    do if((n|32|0)==112)
                    {
                        h=Yd(b,f)|0;
                        e=G;
                        if((h|0)==0&(e|0)==-2147483648)if(!f)
                        {
                            Bd(b,0);
                            s=0.0;
                            i=L;
                            return +s
                        }
                        else
                        {
                            if(!(c[C>>2]|0))
                            {
                                h=0;
                                e=0;
                                break
                            }
                            c[D>>2]=(c[D>>2]|0)+-1;
                            h=0;
                            e=0;
                            break
                        }
                    }
                    else if(!(c[C>>2]|0))
                    {
                        h=0;
                        e=0
                    }
                    else
                    {
                        c[D>>2]=(c[D>>2]|0)+-1;
                        h=0;
                        e=0
                    }
                    while(0);
                    H=$n(o|0,p|0,2)|0;
                    H=Wn(H|0,G|0,-32,-1)|0;
                    e=Wn(H|0,G|0,h|0,e|0)|0;
                    h=G;
                    if(!k)
                    {
                        s=+(J|0)*0.0;
                        i=L;
                        return +s
                    }
                    if((h|0)>0|(h|0)==0&e>>>0>(0-I|0)>>>0)
                    {
                        c[(ob()|0)>>2]=34;
                        s=+(J|0)*17976931348623157.0e292*17976931348623157.0e292;
                        i=L;
                        return +s
                    }
                    H=I+-106|0;
                    F=((H|0)<0)<<31>>31;
                    if((h|0)<(F|0)|(h|0)==(F|0)&e>>>0<H>>>0)
                    {
                        c[(ob()|0)>>2]=34;
                        s=+(J|0)*2.2250738585072014e-308*2.2250738585072014e-308;
                        i=L;
                        return +s
                    }
                    if((k|0)>-1)
                    {
                        do
                        {
                            F=!(g>=.5);
                            H=F&1|k<<1;
                            k=H^1;
                            g=g+(F?g:g+-1.0);
                            e=Wn(e|0,h|0,-1,-1)|0;
                            h=G
                        }
                        while((H|0)>-1);
                        n=e;
                        m=g
                    }
                    else
                    {
                        n=e;
                        m=g
                    }
                    e=Vn(32,0,I|0,((I|0)<0)<<31>>31|0)|0;
                    e=Wn(n|0,h|0,e|0,G|0)|0;
                    I=G;
                    if(0>(I|0)|0==(I|0)&K>>>0>e>>>0)if((e|0)<0)
                    {
                        h=0;
                        A=124
                    }
                    else A=122;
                    else
                    {
                        e=K;
                        A=122
                    }
                    do if((A|0)==122)
                    {
                        if((e|0)<53)
                        {
                            h=e;
                            A=124;
                            break
                        }
                        j=+(J|0);
                        g=0.0
                    }
                    while(0);
                    if((A|0)==124)
                    {
                        g=+(J|0);
                        e=h;
                        j=g;
                        g=+Ed(+Jd(1.0,84-h|0),g)
                    }
                    K=(k&1|0)==0&(m!=0.0&(e|0)<32);
                    g=j*(K?0.0:m)+(g+j*+(((K&1)+k|0)>>>0))-g;
                    if(!(g!=0.0))c[(ob()|0)>>2]=34;
                    s=+Kd(g,n);
                    i=L;
                    return +s
                }
                while(0);
                E=I+K|0;
                F=0-E|0;
                n=0;
                while(1)
                {
                    if((e|0)==46)
                    {
                        A=135;
                        break
                    }
                    else if((e|0)!=48)
                    {
                        h=0;
                        t=0;
                        q=0;
                        break
                    }
                    e=c[D>>2]|0;
                    if(e>>>0<(c[C>>2]|0)>>>0)
                    {
                        c[D>>2]=e+1;
                        e=d[e>>0]|0;
                        n=1;
                        continue
                    }
                    else
                    {
                        e=Cd(b)|0;
                        n=1;
                        continue
                    }
                }
                if((A|0)==135)
                {
                    e=c[D>>2]|0;
                    if(e>>>0<(c[C>>2]|0)>>>0)
                    {
                        c[D>>2]=e+1;
                        e=d[e>>0]|0
                    }
                    else e=Cd(b)|0;
                    if((e|0)==48)
                    {
                        h=0;
                        e=0;
                        while(1)
                        {
                            h=Wn(h|0,e|0,-1,-1)|0;
                            n=G;
                            e=c[D>>2]|0;
                            if(e>>>0<(c[C>>2]|0)>>>0)
                            {
                                c[D>>2]=e+1;
                                e=d[e>>0]|0
                            }
                            else e=Cd(b)|0;
                            if((e|0)==48)e=n;
                            else
                            {
                                t=n;
                                n=1;
                                q=1;
                                break
                            }
                        }
                    }
                    else
                    {
                        h=0;
                        t=0;
                        q=1
                    }
                }
                c[H>>2]=0;
                o=e+-48|0;
                p=(e|0)==46;
                a:do if(p|o>>>0<10)
                {
                    B=H+496|0;
                    y=0;
                    z=0;
                    k=0;
                    l=0;
                    A=0;
                    b:while(1)
                        {
                        do if(p)if(!q)
                        {
                            h=y;
                            t=z;
                            q=1;
                            u=A
                        }
                        else
                        {
                            q=t;
                            e=y;
                            o=z;
                            p=A;
                            break b
                        }
                        else
                        {
                            u=Wn(y|0,z|0,1,0)|0;
                            p=G;
                            x=(e|0)!=48;
                            if((l|0)>=125)
                            {
                                if(!x)
                                {
                                    y=u;
                                    z=p;
                                    u=A;
                                    break
                                }
                                c[B>>2]=c[B>>2]|1;
                                y=u;
                                z=p;
                                u=A;
                                break
                            }
                            n=H+(l<<2)|0;
                            if(k)o=e+-48+((c[n>>2]|0)*10|0)|0;
                            c[n>>2]=o;
                            k=k+1|0;
                            o=(k|0)==9;
                            y=u;
                            z=p;
                            n=1;
                            k=o?0:k;
                            l=(o&1)+l|0;
                            u=x?u:A
                        }
                        while(0);
                        e=c[D>>2]|0;
                        if(e>>>0<(c[C>>2]|0)>>>0)
                        {
                            c[D>>2]=e+1;
                            e=d[e>>0]|0
                        }
                        else e=Cd(b)|0;
                        o=e+-48|0;
                        p=(e|0)==46;
                        if(!(p|o>>>0<10))
                        {
                            o=z;
                            p=u;
                            A=158;
                            break a
                        }
                        else A=u
                    }
                    n=(n|0)!=0;
                    A=166
                }
                else
                {
                    y=0;
                    o=0;
                    k=0;
                    l=0;
                    p=0;
                    A=158
                }
                while(0);
                do if((A|0)==158)
                {
                    q=(q|0)==0;
                    h=q?y:h;
                    q=q?o:t;
                    n=(n|0)!=0;
                    if(!((e|32|0)==101&n))if((e|0)>-1)
                    {
                        e=y;
                        A=166;
                        break
                    }
                    else
                    {
                        e=y;
                        t=n;
                        n=q;
                        A=168;
                        break
                    }
                    e=Yd(b,f)|0;
                    n=G;
                    if((e|0)==0&(n|0)==-2147483648)
                    {
                        if(!f)
                        {
                            Bd(b,0);
                            g=0.0;
                            break
                        }
                        if(!(c[C>>2]|0))
                        {
                            e=0;
                            n=0
                        }
                        else
                        {
                            c[D>>2]=(c[D>>2]|0)+-1;
                            e=0;
                            n=0
                        }
                    }
                    h=Wn(e|0,n|0,h|0,q|0)|0;
                    e=y;
                    q=G;
                    A=170
                }
                while(0);
                if((A|0)==166)if(c[C>>2]|0)
                {
                    c[D>>2]=(c[D>>2]|0)+-1;
                    if(n)A=170;
                    else A=169
                }
                else
                {
                    t=n;
                    n=q;
                    A=168
                }
                if((A|0)==168)if(t)
                {
                    q=n;
                    A=170
                }
                else A=169;
                do if((A|0)==169)
                {
                    c[(ob()|0)>>2]=22;
                    Bd(b,0);
                    g=0.0
                }
                else if((A|0)==170)
                {
                    n=c[H>>2]|0;
                    if(!n)
                    {
                        g=+(J|0)*0.0;
                        break
                    }
                    if(((o|0)<0|(o|0)==0&e>>>0<10)&((h|0)==(e|0)&(q|0)==(o|0))?K>>>0>30|(n>>>K|0)==0:0)
                    {
                        g=+(J|0)*+(n>>>0);
                        break
                    }
                    b=(I|0)/-2|0;
                    D=((b|0)<0)<<31>>31;
                    if((q|0)>(D|0)|(q|0)==(D|0)&h>>>0>b>>>0)
                    {
                        c[(ob()|0)>>2]=34;
                        g=+(J|0)*17976931348623157.0e292*17976931348623157.0e292;
                        break
                    }
                    b=I+-106|0;
                    D=((b|0)<0)<<31>>31;
                    if((q|0)<(D|0)|(q|0)==(D|0)&h>>>0<b>>>0)
                    {
                        c[(ob()|0)>>2]=34;
                        g=+(J|0)*2.2250738585072014e-308*2.2250738585072014e-308;
                        break
                    }
                    if(k)
                    {
                        if((k|0)<9)
                        {
                            n=H+(l<<2)|0;
                            e=c[n>>2]|0;
                            do
                            {
                                e=e*10|0;
                                k=k+1|0
                            }
                            while((k|0)!=9);
                            c[n>>2]=e
                        }
                        l=l+1|0
                    }
                    if((p|0)<9?(p|0)<=(h|0)&(h|0)<18:0)
                    {
                        if((h|0)==9)
                        {
                            g=+(J|0)*+((c[H>>2]|0)>>>0);
                            break
                        }
                        if((h|0)<9)
                        {
                            g=+(J|0)*+((c[H>>2]|0)>>>0)/+(c[1776+(8-h<<2)>>2]|0);
                            break
                        }
                        b=K+27+(ca(h,-3)|0)|0;
                        e=c[H>>2]|0;
                        if((b|0)>30|(e>>>b|0)==0)
                        {
                            g=+(J|0)*+(e>>>0)*+(c[1776+(h+-10<<2)>>2]|0);
                            break
                        }
                    }
                    e=(h|0)%9|0;
                    if(!e)
                    {
                        k=0;
                        e=0
                    }
                    else
                    {
                        q=(h|0)>-1?e:e+9|0;
                        n=c[1776+(8-q<<2)>>2]|0;
                        if(l)
                        {
                            p=1e9/(n|0)|0;
                            k=0;
                            e=0;
                            o=0;
                            do
                            {
                                C=H+(o<<2)|0;
                                D=c[C>>2]|0;
                                b=((D>>>0)/(n>>>0)|0)+e|0;
                                c[C>>2]=b;
                                e=ca((D>>>0)%(n>>>0)|0,p)|0;
                                b=(o|0)==(k|0)&(b|0)==0;
                                o=o+1|0;
                                h=b?h+-9|0:h;
                                k=b?o&127:k
                            }
                            while((o|0)!=(l|0));
                            if(e)
                            {
                                c[H+(l<<2)>>2]=e;
                                l=l+1|0
                            }
                        }
                        else
                        {
                            k=0;
                            l=0
                        }
                        e=0;
                        h=9-q+h|0
                    }
                    c:while(1)
                        {
                        t=H+(k<<2)|0;
                        if((h|0)<18)do
                            {
                            o=0;
                            n=l+127|0;
                            while(1)
                            {
                                q=n&127;
                                p=H+(q<<2)|0;
                                n=$n(c[p>>2]|0,0,29)|0;
                                n=Wn(n|0,G|0,o|0,0)|0;
                                o=G;
                                if(o>>>0>0|(o|0)==0&n>>>0>1e9)
                                {
                                    b=io(n|0,o|0,1e9,0)|0;
                                    n=jo(n|0,o|0,1e9,0)|0;
                                    o=b
                                }
                                else o=0;
                                c[p>>2]=n;
                                b=(q|0)==(k|0);
                                l=(q|0)!=(l+127&127|0)|b?l:(n|0)==0?q:l;
                                if(b)
                                {
                                    n=o;
                                    break
                                }
                                else n=q+-1|0
                            }
                            e=e+-29|0
                        }
                        while((n|0)==0);
                        else
                        {
                            if((h|0)!=18)break;
                            do
                            {
                                if((c[t>>2]|0)>>>0>=9007199)
                                {
                                    h=18;
                                    break c
                                }
                                n=0;
                                o=l+127|0;
                                while(1)
                                {
                                    q=o&127;
                                    p=H+(q<<2)|0;
                                    o=$n(c[p>>2]|0,0,29)|0;
                                    o=Wn(o|0,G|0,n|0,0)|0;
                                    n=G;
                                    if(n>>>0>0|(n|0)==0&o>>>0>1e9)
                                    {
                                        b=io(o|0,n|0,1e9,0)|0;
                                        o=jo(o|0,n|0,1e9,0)|0;
                                        n=b
                                    }
                                    else n=0;
                                    c[p>>2]=o;
                                    b=(q|0)==(k|0);
                                    l=(q|0)!=(l+127&127|0)|b?l:(o|0)==0?q:l;
                                    if(b)break;
                                    else o=q+-1|0
                                }
                                e=e+-29|0
                            }
                            while((n|0)==0)
                        }
                        k=k+127&127;
                        if((k|0)==(l|0))
                        {
                            b=l+127&127;
                            l=H+((l+126&127)<<2)|0;
                            c[l>>2]=c[l>>2]|c[H+(b<<2)>>2];
                            l=b
                        }
                        c[H+(k<<2)>>2]=n;
                        h=h+9|0
                    }
                    d:while(1)
                        {
                        y=l+1&127;
                        x=H+((l+127&127)<<2)|0;
                        while(1)
                        {
                            t=(h|0)==18;
                            u=(h|0)>27?9:1;
                            q=t^1;
                            while(1)
                            {
                                n=k&127;
                                o=(n|0)==(l|0);
                                do if(!o)
                                {
                                    p=c[H+(n<<2)>>2]|0;
                                    if(p>>>0<9007199)
                                    {
                                        A=220;
                                        break
                                    }
                                    if(p>>>0>9007199)break;
                                    p=k+1&127;
                                    if((p|0)==(l|0))
                                    {
                                        A=220;
                                        break
                                    }
                                    p=c[H+(p<<2)>>2]|0;
                                    if(p>>>0<254740991)
                                    {
                                        A=220;
                                        break
                                    }
                                    if(!(p>>>0>254740991|q))break d
                                }
                                else A=220;
                                while(0);
                                if((A|0)==220?(A=0,t):0)
                                {
                                    A=221;
                                    break d
                                }
                                e=e+u|0;
                                if((k|0)==(l|0))k=l;
                                else break
                            }
                            q=(1<<u)+-1|0;
                            t=1e9>>>u;
                            p=k;
                            n=0;
                            o=k;
                            while(1)
                            {
                                D=H+(o<<2)|0;
                                b=c[D>>2]|0;
                                k=(b>>>u)+n|0;
                                c[D>>2]=k;
                                n=ca(b&q,t)|0;
                                k=(o|0)==(p|0)&(k|0)==0;
                                o=o+1&127;
                                h=k?h+-9|0:h;
                                k=k?o:p;
                                if((o|0)==(l|0))break;
                                else p=k
                            }
                            if(!n)continue;
                            if((y|0)!=(k|0))break;
                            c[x>>2]=c[x>>2]|1
                        }
                        c[H+(l<<2)>>2]=n;
                        l=y
                    }
                    if((A|0)==221)if(o)
                    {
                        c[H+(y+-1<<2)>>2]=0;
                        n=l;
                        l=y
                    }
                    g=+((c[H+(n<<2)>>2]|0)>>>0);
                    h=k+1&127;
                    if((h|0)==(l|0))
                    {
                        l=k+2&127;
                        c[H+(l+-1<<2)>>2]=0
                    }
                    s=+(J|0);
                    j=s*(g*1.0e9+ +((c[H+(h<<2)>>2]|0)>>>0));
                    t=e+53|0;
                    o=t-I|0;
                    q=(o|0)<(K|0);
                    h=q&1;
                    p=q?((o|0)<0?0:o):K;
                    if((p|0)<53)
                    {
                        M=+Ed(+Jd(1.0,105-p|0),j);
                        m=+Gd(j,+Jd(1.0,53-p|0));
                        r=M;
                        g=m;
                        m=M+(j-m)
                    }
                    else
                    {
                        r=0.0;
                        g=0.0;
                        m=j
                    }
                    n=k+2&127;
                    do if((n|0)==(l|0))j=g;
                    else
                    {
                        n=c[H+(n<<2)>>2]|0;
                        do if(n>>>0>=5e8)
                        {
                            if(n>>>0>5e8)
                            {
                                g=s*.75+g;
                                break
                            }
                            if((k+3&127|0)==(l|0))
                            {
                                g=s*.5+g;
                                break
                            }
                            else
                            {
                                g=s*.75+g;
                                break
                            }
                        }
                        else
                        {
                            if((n|0)==0?(k+3&127|0)==(l|0):0)break;
                            g=s*.25+g
                        }
                        while(0);
                        if((53-p|0)<=1)
                        {
                            j=g;
                            break
                        }
                        if(+Gd(g,1.0)!=0.0)
                        {
                            j=g;
                            break
                        }
                        j=g+1.0
                    }
                    while(0);
                    g=m+j-r;
                    do if((t&2147483647|0)>(-2-E|0))
                    {
                        if(+R(+g)>=9007199254740992.0)
                        {
                            h=q&(p|0)==(o|0)?0:h;
                            e=e+1|0;
                            g=g*.5
                        }
                        if((e+50|0)<=(F|0)?!(j!=0.0&(h|0)!=0):0)break;
                        c[(ob()|0)>>2]=34
                    }
                    while(0);
                    g=+Kd(g,e)
                }
                while(0);
                s=g;
                i=L;
                return +s
            }
            while(0);
            if(c[C>>2]|0)c[D>>2]=(c[D>>2]|0)+-1;
            c[(ob()|0)>>2]=22;
            Bd(b,0);
            s=0.0;
            i=L;
            return +s
        }
        while(0);
        if((A|0)==23)
        {
            e=(c[C>>2]|0)==0;
            if(!e)c[D>>2]=(c[D>>2]|0)+-1;
            if(!(h>>>0<4|(f|0)==0|e))
            {
                e=c[D>>2]|0;
                do
                {
                    e=e+-1|0;
                    h=h+-1|0
                }
                while(h>>>0>3);
                c[D>>2]=e
            }
        }
        s=+(J|0)*w;
        i=L;
        return +s
    }
    function Bd(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0,f=0;
        c[a+104>>2]=b;
        d=c[a+4>>2]|0;
        e=c[a+8>>2]|0;
        f=e-d|0;
        c[a+108>>2]=f;
        if((b|0)!=0&(f|0)>(b|0))
        {
            c[a+100>>2]=d+b;
            return
        }
        else
        {
            c[a+100>>2]=e;
            return
        }
    }
    function Cd(b)
    {
        b=b|0;
        var e=0,f=0,g=0,h=0,i=0,j=0;
        e=b+104|0;
        g=c[e>>2]|0;
        if(!((g|0)!=0?(c[b+108>>2]|0)>=(g|0):0))i=3;
        if((i|0)==3?(j=Pd(b)|0,(j|0)>=0):0)
        {
            g=c[e>>2]|0;
            e=b+8|0;
            if(g)
            {
                f=c[e>>2]|0;
                h=c[b+4>>2]|0;
                e=f;
                g=g-(c[b+108>>2]|0)+-1|0;
                if((e-h|0)>(g|0))c[b+100>>2]=h+g;
                else i=9
            }
            else
            {
                f=c[e>>2]|0;
                e=f;
                i=9
            }
            if((i|0)==9)c[b+100>>2]=e;
            e=c[b+4>>2]|0;
            if(f)
            {
                b=b+108|0;
                c[b>>2]=f+1-e+(c[b>>2]|0)
            }
            e=e+-1|0;
            if((d[e>>0]|0|0)==(j|0))return j|0;
            a[e>>0]=j;
            return j|0
        }
        c[b+100>>2]=0;
        j=-1;
        return j|0
    }
    function Dd(a,b)
    {
        a=+a;
        b=+b;
        var d=0,e=0;
        h[k>>3]=a;
        e=c[k>>2]|0;
        d=c[k+4>>2]|0;
        h[k>>3]=b;
        d=c[k+4>>2]&-2147483648|d&2147483647;
        c[k>>2]=e;
        c[k+4>>2]=d;
        return +(+h[k>>3])
    }
    function Ed(a,b)
    {
        a=+a;
        b=+b;
        return +(+Dd(a,b))
    }
    function Fd(a,b)
    {
        a=+a;
        b=+b;
        var d=0,e=0,f=0,g=0,i=0,j=0,l=0,m=0,n=0,o=0,p=0;
        h[k>>3]=a;
        l=c[k>>2]|0;
        m=c[k+4>>2]|0;
        h[k>>3]=b;
        n=c[k>>2]|0;
        o=c[k+4>>2]|0;
        e=Zn(l|0,m|0,52)|0;
        e=e&2047;
        g=Zn(n|0,o|0,52)|0;
        g=g&2047;
        p=m&-2147483648;
        i=$n(n|0,o|0,1)|0;
        j=G;
        if(!((i|0)==0&(j|0)==0)?(f=o&2147483647,!(f>>>0>2146435072|(f|0)==2146435072&n>>>0>0|(e|0)==2047)):0)
        {
            d=$n(l|0,m|0,1)|0;
            f=G;
            if(!(f>>>0>j>>>0|(f|0)==(j|0)&d>>>0>i>>>0))return +((d|0)==(i|0)&(f|0)==(j|0)?a*0.0:a);
            if(!e)
            {
                d=$n(l|0,m|0,12)|0;
                e=G;
                if((e|0)>-1|(e|0)==-1&d>>>0>4294967295)
                {
                    f=e;
                    e=0;
                    do
                    {
                        e=e+-1|0;
                        d=$n(d|0,f|0,1)|0;
                        f=G
                    }
                    while((f|0)>-1|(f|0)==-1&d>>>0>4294967295)
                }
                else e=0;
                l=$n(l|0,m|0,1-e|0)|0;
                j=G
            }
            else j=m&1048575|1048576;
            if(!g)
            {
                d=$n(n|0,o|0,12)|0;
                f=G;
                if((f|0)>-1|(f|0)==-1&d>>>0>4294967295)
                {
                    g=0;
                    do
                    {
                        g=g+-1|0;
                        d=$n(d|0,f|0,1)|0;
                        f=G
                    }
                    while((f|0)>-1|(f|0)==-1&d>>>0>4294967295)
                }
                else g=0;
                n=$n(n|0,o|0,1-g|0)|0;
                m=G
            }
            else m=o&1048575|1048576;
            d=Vn(l|0,j|0,n|0,m|0)|0;
            f=G;
            i=(f|0)>-1|(f|0)==-1&d>>>0>4294967295;
            a:do if((e|0)>(g|0))
            {
                while(1)
                {
                    if(i)
                    {
                        if((l|0)==(n|0)&(j|0)==(m|0))break
                    }
                    else
                    {
                        d=l;
                        f=j
                    }
                    l=$n(d|0,f|0,1)|0;
                    j=G;
                    e=e+-1|0;
                    d=Vn(l|0,j|0,n|0,m|0)|0;
                    f=G;
                    i=(f|0)>-1|(f|0)==-1&d>>>0>4294967295;
                    if((e|0)<=(g|0))break a
                }
                a=a*0.0;
                return +a
            }
            while(0);
            if(i)
            {
                if((l|0)==(n|0)&(j|0)==(m|0))
                {
                    a=a*0.0;
                    return +a
                }
            }
            else
            {
                f=j;
                d=l
            }
            if(f>>>0<1048576|(f|0)==1048576&d>>>0<0)do
                {
                d=$n(d|0,f|0,1)|0;
                f=G;
                e=e+-1|0
            }
            while(f>>>0<1048576|(f|0)==1048576&d>>>0<0);
            if((e|0)>0)
            {
                o=Wn(d|0,f|0,0,-1048576)|0;
                d=G;
                e=$n(e|0,0,52)|0;
                d=d|G;
                e=o|e
            }
            else
            {
                e=Zn(d|0,f|0,1-e|0)|0;
                d=G
            }
            c[k>>2]=e;
            c[k+4>>2]=d|p;
            a=+h[k>>3];
            return +a
        }
        a=a*b;
        a=a/a;
        return +a
    }
    function Gd(a,b)
    {
        a=+a;
        b=+b;
        return +(+Fd(a,b))
    }
    function Hd(a,b)
    {
        a=+a;
        b=b|0;
        var d=0,e=0,f=0;
        h[k>>3]=a;
        d=c[k>>2]|0;
        e=c[k+4>>2]|0;
        f=Zn(d|0,e|0,52)|0;
        f=f&2047;
        if((f|0)==2047)return +a;
        else if(!f)
        {
            if(a!=0.0)
            {
                a=+Hd(a*18446744073709552.0e3,b);
                d=(c[b>>2]|0)+-64|0
            }
            else d=0;
            c[b>>2]=d;
            return +a
        }
        else
        {
            c[b>>2]=f+-1022;
            c[k>>2]=d;
            c[k+4>>2]=e&-2146435073|1071644672;
            a=+h[k>>3];
            return +a
        }
        return 0.0
    }
    function Id(a,b)
    {
        a=+a;
        b=b|0;
        return +(+Hd(a,b))
    }
    function Jd(a,b)
    {
        a=+a;
        b=b|0;
        var d=0;
        if((b|0)>1023)
        {
            a=a*89884656743115795.0e291;
            d=b+-1023|0;
            if((d|0)>1023)
            {
                d=b+-2046|0;
                d=(d|0)>1023?1023:d;
                a=a*89884656743115795.0e291
            }
        }
        else if((b|0)<-1022)
        {
            a=a*2.2250738585072014e-308;
            d=b+1022|0;
            if((d|0)<-1022)
            {
                d=b+2044|0;
                d=(d|0)<-1022?-1022:d;
                a=a*2.2250738585072014e-308
            }
        }
        else d=b;
        d=$n(d+1023|0,0,52)|0;
        b=G;
        c[k>>2]=d;
        c[k+4>>2]=b;
        return +(a*+h[k>>3])
    }
    function Kd(a,b)
    {
        a=+a;
        b=b|0;
        return +(+Jd(a,b))
    }
    function Ld(a,b)
    {
        a=a|0;
        b=b|0;
        if(!a)a=0;
        else a=Md(a,b,0)|0;
        return a|0
    }
    function Md(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        if(!b)
        {
            d=1;
            return d|0
        }
        if(d>>>0<128)
        {
            a[b>>0]=d;
            d=1;
            return d|0
        }
        if(d>>>0<2048)
        {
            a[b>>0]=d>>>6|192;
            a[b+1>>0]=d&63|128;
            d=2;
            return d|0
        }
        if(d>>>0<55296|(d&-8192|0)==57344)
        {
            a[b>>0]=d>>>12|224;
            a[b+1>>0]=d>>>6&63|128;
            a[b+2>>0]=d&63|128;
            d=3;
            return d|0
        }
        if((d+-65536|0)>>>0<1048576)
        {
            a[b>>0]=d>>>18|240;
            a[b+1>>0]=d>>>12&63|128;
            a[b+2>>0]=d>>>6&63|128;
            a[b+3>>0]=d&63|128;
            d=4;
            return d|0
        }
        else
        {
            c[(ob()|0)>>2]=84;
            d=-1;
            return d|0
        }
        return 0
    }
    function Nd(b)
    {
        b=b|0;
        var d=0,e=0;
        d=b+74|0;
        e=a[d>>0]|0;
        a[d>>0]=e+255|e;
        d=b+20|0;
        e=b+44|0;
        if((c[d>>2]|0)>>>0>(c[e>>2]|0)>>>0)Jb[c[b+36>>2]&31](b,0,0)|0;
        c[b+16>>2]=0;
        c[b+28>>2]=0;
        c[d>>2]=0;
        d=c[b>>2]|0;
        if(!(d&20))
        {
            e=c[e>>2]|0;
            c[b+8>>2]=e;
            c[b+4>>2]=e;
            b=0;
            return b|0
        }
        if(!(d&4))
        {
            b=-1;
            return b|0
        }
        c[b>>2]=d|32;
        b=-1;
        return b|0
    }
    function Od(b)
    {
        b=b|0;
        var d=0,e=0;
        d=b+74|0;
        e=a[d>>0]|0;
        a[d>>0]=e+255|e;
        d=c[b>>2]|0;
        if(!(d&8))
        {
            c[b+8>>2]=0;
            c[b+4>>2]=0;
            e=c[b+44>>2]|0;
            c[b+28>>2]=e;
            c[b+20>>2]=e;
            c[b+16>>2]=e+(c[b+48>>2]|0);
            e=0;
            return e|0
        }
        else
        {
            c[b>>2]=d|32;
            e=-1;
            return e|0
        }
        return 0
    }
    function Pd(a)
    {
        a=a|0;
        var b=0,e=0;
        e=i;
        i=i+16|0;
        b=e;
        if((c[a+8>>2]|0)==0?(Nd(a)|0)!=0:0)b=-1;
        else if((Jb[c[a+32>>2]&31](a,b,1)|0)==1)b=d[b>>0]|0;
        else b=-1;
        i=e;
        return b|0
    }
    function Qd(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,i=0;
        f=e+16|0;
        g=c[f>>2]|0;
        do if(!g)if(!(Od(e)|0))
        {
            g=c[f>>2]|0;
            break
        }
        else
        {
            i=0;
            return i|0
        }
        while(0);
        i=e+20|0;
        h=c[i>>2]|0;
        if((g-h|0)>>>0<d>>>0)
        {
            i=Jb[c[e+36>>2]&31](e,b,d)|0;
            return i|0
        }
        a:do if((a[e+75>>0]|0)>-1)
        {
            f=d;
            while(1)
            {
                if(!f)
                {
                    g=h;
                    f=0;
                    break a
                }
                g=f+-1|0;
                if((a[b+g>>0]|0)==10)break;
                else f=g
            }
            if((Jb[c[e+36>>2]&31](e,b,f)|0)>>>0<f>>>0)
            {
                i=f;
                return i|0
            }
            else
            {
                d=d-f|0;
                b=b+f|0;
                g=c[i>>2]|0;
                break
            }
        }
        else
        {
            g=h;
            f=0
        }
        while(0);
        _n(g|0,b|0,d|0)|0;
        c[i>>2]=(c[i>>2]|0)+d;
        i=f+d|0;
        return i|0
    }
    function Rd(a,b,d,e)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0;
        f=i;
        i=i+16|0;
        g=f;
        c[g>>2]=e;
        a=Td(a,b,d,g)|0;
        i=f;
        return a|0
    }
    function Sd(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        p=i;
        i=i+224|0;
        l=p+120|0;
        o=p+80|0;
        n=p;
        m=p+136|0;
        e=o;
        f=e+40|0;
        do
        {
            c[e>>2]=0;
            e=e+4|0
        }
        while((e|0)<(f|0));
        c[l>>2]=c[d>>2];
        if((Zd(0,b,l,n,o)|0)<0)
        {
            a=-1;
            i=p;
            return a|0
        }
        f=a+48|0;
        if(!(c[f>>2]|0))
        {
            g=a+44|0;
            h=c[g>>2]|0;
            c[g>>2]=m;
            j=a+28|0;
            c[j>>2]=m;
            k=a+20|0;
            c[k>>2]=m;
            c[f>>2]=80;
            e=a+16|0;
            c[e>>2]=m+80;
            d=Zd(a,b,l,n,o)|0;
            if(h)
            {
                Jb[c[a+36>>2]&31](a,0,0)|0;
                d=(c[k>>2]|0)==0?-1:d;
                c[g>>2]=h;
                c[f>>2]=0;
                c[e>>2]=0;
                c[j>>2]=0;
                c[k>>2]=0
            }
        }
        else d=Zd(a,b,l,n,o)|0;
        a=d;
        i=p;
        return a|0
    }
    function Td(b,d,e,f)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        f=f|0;
        var g=0,h=0,j=0,k=0,l=0,m=0;
        m=i;
        i=i+128|0;
        g=m+112|0;
        l=m;
        h=l;
        j=1808;
        k=h+112|0;
        do
        {
            c[h>>2]=c[j>>2];
            h=h+4|0;
            j=j+4|0
        }
        while((h|0)<(k|0));
        if((d+-1|0)>>>0>2147483646)if(!d)
        {
            b=g;
            d=1
        }
        else
        {
            c[(ob()|0)>>2]=75;
            f=-1;
            i=m;
            return f|0
        }
        j=-2-b|0;
        j=d>>>0>j>>>0?j:d;
        c[l+48>>2]=j;
        g=l+20|0;
        c[g>>2]=b;
        c[l+44>>2]=b;
        d=b+j|0;
        b=l+16|0;
        c[b>>2]=d;
        c[l+28>>2]=d;
        d=Sd(l,e,f)|0;
        if(!j)
        {
            f=d;
            i=m;
            return f|0
        }
        f=c[g>>2]|0;
        a[f+(((f|0)==(c[b>>2]|0))<<31>>31)>>0]=0;
        f=d;
        i=m;
        return f|0
    }
    function Ud(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0.0,f=0,g=0,h=0;
        h=i;
        i=i+112|0;
        g=h;
        d=g;
        f=d+112|0;
        do
        {
            c[d>>2]=0;
            d=d+4|0
        }
        while((d|0)<(f|0));
        f=g+4|0;
        c[f>>2]=a;
        d=g+8|0;
        c[d>>2]=-1;
        c[g+44>>2]=a;
        c[g+76>>2]=-1;
        Bd(g,0);
        e=+Ad(g,2,1);
        d=(c[f>>2]|0)-(c[d>>2]|0)+(c[g+108>>2]|0)|0;
        if(!b)
        {
            i=h;
            return +e
        }
        c[b>>2]=(d|0)!=0?a+d|0:a;
        i=h;
        return +e
    }
    function Vd(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0;
        j=i;
        i=i+112|0;
        h=j;
        c[h>>2]=0;
        f=h+4|0;
        c[f>>2]=a;
        c[h+44>>2]=a;
        g=h+8|0;
        c[g>>2]=(a|0)<0?-1:a+2147483647|0;
        c[h+76>>2]=-1;
        Bd(h,0);
        d=zd(h,d,1,-1,-1)|0;
        e=G;
        if(!b)
        {
            G=e;
            i=j;
            return d|0
        }
        c[b>>2]=a+((c[f>>2]|0)+(c[h+108>>2]|0)-(c[g>>2]|0));
        G=e;
        i=j;
        return d|0
    }
    function Wd(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0;
        j=i;
        i=i+112|0;
        h=j;
        c[h>>2]=0;
        f=h+4|0;
        c[f>>2]=a;
        c[h+44>>2]=a;
        g=h+8|0;
        c[g>>2]=(a|0)<0?-1:a+2147483647|0;
        c[h+76>>2]=-1;
        Bd(h,0);
        d=zd(h,d,1,0,-2147483648)|0;
        e=G;
        if(!b)
        {
            G=e;
            i=j;
            return d|0
        }
        c[b>>2]=a+((c[f>>2]|0)+(c[h+108>>2]|0)-(c[g>>2]|0));
        G=e;
        i=j;
        return d|0
    }
    function Xd(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0,i=0;
        h=d&255;
        f=(e|0)!=0;
        a:do if(f&(b&3|0)!=0)
        {
            g=d&255;
            while(1)
            {
                if((a[b>>0]|0)==g<<24>>24)
                {
                    i=6;
                    break a
                }
                b=b+1|0;
                e=e+-1|0;
                f=(e|0)!=0;
                if(!(f&(b&3|0)!=0))
                {
                    i=5;
                    break
                }
            }
        }
        else i=5;
        while(0);
        if((i|0)==5)if(f)i=6;
        else e=0;
        b:do if((i|0)==6)
        {
            g=d&255;
            if((a[b>>0]|0)!=g<<24>>24)
            {
                f=ca(h,16843009)|0;
                c:do if(e>>>0>3)while(1)
                    {
                    h=c[b>>2]^f;
                    if((h&-2139062144^-2139062144)&h+-16843009)break;
                    b=b+4|0;
                    e=e+-4|0;
                    if(e>>>0<=3)
                    {
                        i=11;
                        break c
                    }
                }
                else i=11;
                while(0);
                if((i|0)==11)if(!e)
                {
                    e=0;
                    break
                }
                while(1)
                {
                    if((a[b>>0]|0)==g<<24>>24)break b;
                    b=b+1|0;
                    e=e+-1|0;
                    if(!e)
                    {
                        e=0;
                        break
                    }
                }
            }
        }
        while(0);
        return ((e|0)!=0?b:0)|0
    }
    function Yd(a,b)
    {
        a=a|0;
        b=b|0;
        var e=0,f=0,g=0,h=0,i=0;
        h=a+4|0;
        e=c[h>>2]|0;
        i=a+100|0;
        if(e>>>0<(c[i>>2]|0)>>>0)
        {
            c[h>>2]=e+1;
            e=d[e>>0]|0
        }
        else e=Cd(a)|0;
        if((e|0)==43|(e|0)==45)
        {
            f=(e|0)==45&1;
            e=c[h>>2]|0;
            if(e>>>0<(c[i>>2]|0)>>>0)
            {
                c[h>>2]=e+1;
                e=d[e>>0]|0
            }
            else e=Cd(a)|0;
            if((b|0)!=0&(e+-48|0)>>>0>9?(c[i>>2]|0)!=0:0)
            {
                c[h>>2]=(c[h>>2]|0)+-1;
                g=f
            }
            else g=f
        }
        else g=0;
        if((e+-48|0)>>>0>9)
        {
            if(!(c[i>>2]|0))
            {
                i=-2147483648;
                a=0;
                G=i;
                return a|0
            }
            c[h>>2]=(c[h>>2]|0)+-1;
            i=-2147483648;
            a=0;
            G=i;
            return a|0
        }
        else f=0;
        do
        {
            f=e+-48+(f*10|0)|0;
            e=c[h>>2]|0;
            if(e>>>0<(c[i>>2]|0)>>>0)
            {
                c[h>>2]=e+1;
                e=d[e>>0]|0
            }
            else e=Cd(a)|0
        }
        while((e+-48|0)>>>0<10&(f|0)<214748364);
        b=((f|0)<0)<<31>>31;
        if((e+-48|0)>>>0<10)do
            {
            b=ho(f|0,b|0,10,0)|0;
            f=G;
            e=Wn(e|0,((e|0)<0)<<31>>31|0,-48,-1)|0;
            f=Wn(e|0,G|0,b|0,f|0)|0;
            b=G;
            e=c[h>>2]|0;
            if(e>>>0<(c[i>>2]|0)>>>0)
            {
                c[h>>2]=e+1;
                e=d[e>>0]|0
            }
            else e=Cd(a)|0
        }
        while((e+-48|0)>>>0<10&((b|0)<21474836|(b|0)==21474836&f>>>0<2061584302));
        if((e+-48|0)>>>0<10)do
            {
            e=c[h>>2]|0;
            if(e>>>0<(c[i>>2]|0)>>>0)
            {
                c[h>>2]=e+1;
                e=d[e>>0]|0
            }
            else e=Cd(a)|0
        }
        while((e+-48|0)>>>0<10);
        if(c[i>>2]|0)c[h>>2]=(c[h>>2]|0)+-1;
        h=(g|0)!=0;
        a=Vn(0,0,f|0,b|0)|0;
        i=h?G:b;
        a=h?a:f;
        G=i;
        return a|0
    }
    function Zd(e,f,g,j,l)
    {
        e=e|0;
        f=f|0;
        g=g|0;
        j=j|0;
        l=l|0;
        var m=0,n=0,o=0,p=0,q=0,r=0,s=0.0,t=0,u=0,v=0,w=0,x=0,y=0.0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ba=0,da=0,ea=0,fa=0,ga=0,ha=0,ia=0,ja=0,ka=0,la=0,ma=0,na=0,oa=0,pa=0,qa=0,ra=0,sa=0,ta=0,ua=0,va=0,wa=0,xa=0,ya=0,za=0,Aa=0,Ba=0,Ca=0,Da=0,Ea=0,Fa=0,Ga=0,Ha=0,Ia=0,Ja=0,Ka=0,La=0,Ma=0,Na=0,Oa=0,Pa=0,Qa=0,Ra=0,Sa=0,Ta=0,Ua=0,Va=0,Wa=0,Xa=0,Ya=0,Za=0,_a=0,$a=0,ab=0,bb=0,cb=0,db=0;
        db=i;
        i=i+864|0;
        Ma=db+16|0;
        Pa=db;
        Na=db+832|0;
        la=Na;
        Ja=db+816|0;
        Wa=db+520|0;
        Fa=db+776|0;
        ab=db+8|0;
        Ta=db+828|0;
        ma=(e|0)!=0;
        za=Fa+40|0;
        Ca=za;
        Fa=Fa+39|0;
        Ga=ab+4|0;
        Ha=ab;
        Ia=Ja+12|0;
        Ja=Ja+11|0;
        Ka=Ia;
        na=Ka-la|0;
        oa=-2-la|0;
        ua=Ka+2|0;
        va=Ma+288|0;
        wa=Na+9|0;
        xa=wa;
        ya=Na+8|0;
        A=0;
        B=0;
        u=0;
        q=0;
        v=0;
        a:while(1)
            {
            do if((u|0)>-1)if((q|0)>(2147483647-u|0))
            {
                c[(ob()|0)>>2]=75;
                $=-1;
                break
            }
            else
            {
                $=q+u|0;
                break
            }
            else $=u;
            while(0);
            q=a[f>>0]|0;
            if(!(q<<24>>24))
            {
                Oa=$;
                Sa=v;
                O=344;
                break
            }
            else p=f;
            while(1)
            {
                if(!(q<<24>>24))
                {
                    ja=p;
                    ea=p;
                    break
                }
                else if(q<<24>>24==37)
                {
                    Ra=p;
                    bb=p;
                    O=9;
                    break
                }
                M=p+1|0;
                q=a[M>>0]|0;
                p=M
            }
            b:do if((O|0)==9)while(1)
                {
                O=0;
                if((a[Ra+1>>0]|0)!=37)
                {
                    ja=Ra;
                    ea=bb;
                    break b
                }
                p=bb+1|0;
                q=Ra+2|0;
                if((a[q>>0]|0)==37)
                {
                    Ra=q;
                    bb=p
                }
                else
                {
                    ja=q;
                    ea=p;
                    break
                }
            }
            while(0);
            q=ea-f|0;
            if(ma)Qd(f,q,e)|0;
            if((ea|0)!=(f|0))
            {
                u=$;
                f=ja;
                continue
            }
            t=ja+1|0;
            p=a[t>>0]|0;
            r=(p<<24>>24)+-48|0;
            if(r>>>0<10)
            {
                M=(a[ja+2>>0]|0)==36;
                t=M?ja+3|0:t;
                p=a[t>>0]|0;
                z=M?r:-1;
                v=M?1:v
            }
            else z=-1;
            r=p<<24>>24;
            c:do if((r&-32|0)==32)
            {
                u=0;
                do
                {
                    if(!(1<<r+-32&75913))break c;
                    u=1<<(p<<24>>24)+-32|u;
                    t=t+1|0;
                    p=a[t>>0]|0;
                    r=p<<24>>24
                }
                while((r&-32|0)==32)
            }
            else u=0;
            while(0);
            do if(p<<24>>24==42)
            {
                r=t+1|0;
                p=(a[r>>0]|0)+-48|0;
                if(p>>>0<10?(a[t+2>>0]|0)==36:0)
                {
                    c[l+(p<<2)>>2]=10;
                    v=1;
                    p=t+3|0;
                    t=c[j+((a[r>>0]|0)+-48<<3)>>2]|0
                }
                else
                {
                    if(v)
                    {
                        cb=-1;
                        O=363;
                        break a
                    }
                    if(!ma)
                    {
                        p=r;
                        v=0;
                        M=0;
                        break
                    }
                    v=(c[g>>2]|0)+(4-1)&~(4-1);
                    t=c[v>>2]|0;
                    c[g>>2]=v+4;
                    v=0;
                    p=r
                }
                if((t|0)<0)
                {
                    u=u|8192;
                    M=0-t|0
                }
                else M=t
            }
            else
            {
                r=(p<<24>>24)+-48|0;
                if(r>>>0<10)
                {
                    p=t;
                    t=0;
                    do
                    {
                        t=(t*10|0)+r|0;
                        p=p+1|0;
                        r=(a[p>>0]|0)+-48|0
                    }
                    while(r>>>0<10);
                    if((t|0)<0)
                    {
                        cb=-1;
                        O=363;
                        break a
                    }
                    else M=t
                }
                else
                {
                    p=t;
                    M=0
                }
            }
            while(0);
            d:do if((a[p>>0]|0)==46)
            {
                t=p+1|0;
                r=a[t>>0]|0;
                if(r<<24>>24!=42)
                {
                    r=(r<<24>>24)+-48|0;
                    if(r>>>0<10)
                    {
                        p=t;
                        t=0
                    }
                    else
                    {
                        p=t;
                        C=0;
                        break
                    }
                    while(1)
                    {
                        t=(t*10|0)+r|0;
                        p=p+1|0;
                        r=(a[p>>0]|0)+-48|0;
                        if(r>>>0>=10)
                        {
                            C=t;
                            break d
                        }
                    }
                }
                r=p+2|0;
                t=(a[r>>0]|0)+-48|0;
                if(t>>>0<10?(a[p+3>>0]|0)==36:0)
                {
                    c[l+(t<<2)>>2]=10;
                    p=p+4|0;
                    C=c[j+((a[r>>0]|0)+-48<<3)>>2]|0;
                    break
                }
                if(v)
                {
                    cb=-1;
                    O=363;
                    break a
                }
                if(ma)
                {
                    p=(c[g>>2]|0)+(4-1)&~(4-1);
                    C=c[p>>2]|0;
                    c[g>>2]=p+4;
                    p=r
                }
                else
                {
                    p=r;
                    C=0
                }
            }
            else C=-1;
            while(0);
            x=0;
            while(1)
            {
                t=(a[p>>0]|0)+-65|0;
                if(t>>>0>57)
                {
                    cb=-1;
                    O=363;
                    break a
                }
                r=p+1|0;
                t=a[1920+(x*58|0)+t>>0]|0;
                w=t&255;
                if((w+-1|0)>>>0<8)
                {
                    p=r;
                    x=w
                }
                else
                {
                    N=r;
                    break
                }
            }
            if(!(t<<24>>24))
            {
                cb=-1;
                O=363;
                break
            }
            r=(z|0)>-1;
            e:do if(t<<24>>24==19)if(r)
            {
                cb=-1;
                O=363;
                break a
            }
            else
            {
                pa=A;
                qa=B;
                O=62
            }
            else
            {
                if(r)
                {
                    c[l+(z<<2)>>2]=w;
                    qa=j+(z<<3)|0;
                    pa=c[qa+4>>2]|0;
                    qa=c[qa>>2]|0;
                    O=62;
                    break
                }
                if(!ma)
                {
                    cb=0;
                    O=363;
                    break a
                }
                if((t&255)>20)
                {
                    Aa=B;
                    Ba=A
                }
                else do switch(w|0)
                    {
                    case 11:
                        {
                            Ba=(c[g>>2]|0)+(4-1)&~(4-1);
                            Aa=c[Ba>>2]|0;
                            c[g>>2]=Ba+4;
                            Ba=0;
                            break e
                        }
                    case 12:
                        {
                            L=(c[g>>2]|0)+(8-1)&~(8-1);
                            Ba=L;
                            Aa=c[Ba>>2]|0;
                            Ba=c[Ba+4>>2]|0;
                            c[g>>2]=L+8;
                            break e
                        }
                    case 10:
                        {
                            Aa=(c[g>>2]|0)+(4-1)&~(4-1);
                            Ba=c[Aa>>2]|0;
                            c[g>>2]=Aa+4;
                            Aa=Ba;
                            Ba=((Ba|0)<0)<<31>>31;
                            break e
                        }
                    case 13:
                        {
                            Aa=(c[g>>2]|0)+(4-1)&~(4-1);
                            Ba=c[Aa>>2]|0;
                            c[g>>2]=Aa+4;
                            Aa=Ba<<16>>16;
                            Ba=(((Ba&65535)<<16>>16|0)<0)<<31>>31;
                            break e
                        }
                    case 16:
                        {
                            Ba=(c[g>>2]|0)+(4-1)&~(4-1);
                            Aa=c[Ba>>2]|0;
                            c[g>>2]=Ba+4;
                            Aa=Aa&255;
                            Ba=0;
                            break e
                        }
                    case 9:
                        {
                            Ba=(c[g>>2]|0)+(4-1)&~(4-1);
                            Aa=c[Ba>>2]|0;
                            c[g>>2]=Ba+4;
                            Ba=A;
                            break e
                        }
                    case 14:
                        {
                            Ba=(c[g>>2]|0)+(4-1)&~(4-1);
                            Aa=c[Ba>>2]|0;
                            c[g>>2]=Ba+4;
                            Aa=Aa&65535;
                            Ba=0;
                            break e
                        }
                    case 17:
                        {
                            Aa=(c[g>>2]|0)+(8-1)&~(8-1);
                            y=+h[Aa>>3];
                            c[g>>2]=Aa+8;
                            h[k>>3]=y;
                            Aa=c[k>>2]|0;
                            Ba=c[k+4>>2]|0;
                            break e
                        }
                    case 18:
                        {
                            Aa=(c[g>>2]|0)+(8-1)&~(8-1);
                            y=+h[Aa>>3];
                            c[g>>2]=Aa+8;
                            h[k>>3]=y;
                            Aa=c[k>>2]|0;
                            Ba=c[k+4>>2]|0;
                            break e
                        }
                    case 15:
                        {
                            Aa=(c[g>>2]|0)+(4-1)&~(4-1);
                            Ba=c[Aa>>2]|0;
                            c[g>>2]=Aa+4;
                            Aa=Ba<<24>>24;
                            Ba=(((Ba&255)<<24>>24|0)<0)<<31>>31;
                            break e
                        }
                    default:
                        {
                            Aa=B;
                            Ba=A;
                            break e
                        }
                }
                while(0)
            }
            while(0);
            if((O|0)==62)
            {
                O=0;
                if(ma)
                {
                    Aa=qa;
                    Ba=pa
                }
                else
                {
                    A=pa;
                    B=qa;
                    u=$;
                    f=N;
                    continue
                }
            }
            H=a[p>>0]|0;
            H=(x|0)!=0&(H&15|0)==3?H&-33:H;
            t=u&-65537;
            L=(u&8192|0)==0?u:t;
            f:do switch(H|0)
                {
                case 65:case 71:case 70:case 69:case 97:case 103:case 102:case 101:
                    {
                        c[k>>2]=Aa;
                        c[k+4>>2]=Ba;
                        s=+h[k>>3];
                        c[Pa>>2]=0;
                        if((Ba|0)>=0)if(!(L&2048))
                        {
                            J=L&1;
                            I=J;
                            J=(J|0)==0?2425:2430
                        }
                        else
                        {
                            I=1;
                            J=2427
                        }
                        else
                        {
                            s=-s;
                            I=1;
                            J=2424
                        }
                        h[k>>3]=s;
                        K=c[k+4>>2]&2146435072;
                        do if(K>>>0<2146435072|(K|0)==2146435072&0<0)
                        {
                            y=+Id(s,Pa)*2.0;
                            t=y!=0.0;
                            if(t)c[Pa>>2]=(c[Pa>>2]|0)+-1;
                            D=H|32;
                            if((D|0)==97)
                            {
                                p=H&32;
                                B=(p|0)==0?J:J+9|0;
                                A=I|2;
                                f=12-C|0;
                                do if(!(C>>>0>11|(f|0)==0))
                                {
                                    s=8.0;
                                    do
                                    {
                                        f=f+-1|0;
                                        s=s*16.0
                                    }
                                    while((f|0)!=0);
                                    if((a[B>>0]|0)==45)
                                    {
                                        s=-(s+(-y-s));
                                        break
                                    }
                                    else
                                    {
                                        s=y+s-s;
                                        break
                                    }
                                }
                                else s=y;
                                while(0);
                                t=c[Pa>>2]|0;
                                t=(t|0)<0?0-t|0:t;
                                if((t|0)<0)
                                {
                                    f=Ia;
                                    q=t;
                                    u=((t|0)<0)<<31>>31;
                                    while(1)
                                    {
                                        t=jo(q|0,u|0,10,0)|0;
                                        f=f+-1|0;
                                        a[f>>0]=t|48;
                                        t=io(q|0,u|0,10,0)|0;
                                        if(u>>>0>9|(u|0)==9&q>>>0>4294967295)
                                        {
                                            q=t;
                                            u=G
                                        }
                                        else break
                                    }
                                }
                                else f=Ia;
                                if(t)while(1)
                                    {
                                    f=f+-1|0;
                                    a[f>>0]=(t>>>0)%10|0|48;
                                    if(t>>>0<10)break;
                                    else t=(t>>>0)/10|0
                                }
                                if((f|0)==(Ia|0))
                                {
                                    a[Ja>>0]=48;
                                    f=Ja
                                }
                                a[f+-1>>0]=(c[Pa>>2]>>31&2)+43;
                                z=f+-2|0;
                                a[z>>0]=H+15;
                                if(!(L&8))if((C|0)<1)
                                {
                                    f=Na;
                                    do
                                    {
                                        K=~~s;
                                        t=f+1|0;
                                        a[f>>0]=d[2384+K>>0]|p;
                                        s=(s-+(K|0))*16.0;
                                        if((t-la|0)!=1|s==0.0)f=t;
                                        else
                                        {
                                            a[t>>0]=46;
                                            f=f+2|0
                                        }
                                    }
                                    while(s!=0.0)
                                }
                                else
                                {
                                    f=Na;
                                    do
                                    {
                                        K=~~s;
                                        t=f+1|0;
                                        a[f>>0]=d[2384+K>>0]|p;
                                        s=(s-+(K|0))*16.0;
                                        if((t-la|0)==1)
                                        {
                                            a[t>>0]=46;
                                            f=f+2|0
                                        }
                                        else f=t
                                    }
                                    while(s!=0.0)
                                }
                                else
                                {
                                    f=Na;
                                    do
                                    {
                                        K=~~s;
                                        t=f+1|0;
                                        a[f>>0]=d[2384+K>>0]|p;
                                        s=(s-+(K|0))*16.0;
                                        if((t-la|0)==1)
                                        {
                                            a[t>>0]=46;
                                            f=f+2|0
                                        }
                                        else f=t
                                    }
                                    while(s!=0.0)
                                }
                                u=(C|0)!=0&(oa+f|0)<(C|0)?ua+C-z|0:na-z+f|0;
                                x=u+A|0;
                                r=L&73728;
                                w=(M|0)>(x|0);
                                if((r|0)==0&w)
                                {
                                    t=M-x|0;
                                    Xn(Wa|0,32,(t>>>0>256?256:t)|0)|0;
                                    if(t>>>0>255)
                                    {
                                        q=t;
                                        do
                                        {
                                            Qd(Wa,256,e)|0;
                                            q=q+-256|0
                                        }
                                        while(q>>>0>255);
                                        t=t&255
                                    }
                                    Qd(Wa,t,e)|0
                                }
                                Qd(B,A,e)|0;
                                if((r|0)==65536&w)
                                {
                                    q=M-x|0;
                                    Xn(Wa|0,48,(q>>>0>256?256:q)|0)|0;
                                    if(q>>>0>255)
                                    {
                                        p=q;
                                        do
                                        {
                                            Qd(Wa,256,e)|0;
                                            p=p+-256|0
                                        }
                                        while(p>>>0>255);
                                        q=q&255
                                    }
                                    Qd(Wa,q,e)|0
                                }
                                f=f-la|0;
                                Qd(Na,f,e)|0;
                                t=Ka-z|0;
                                f=u-t-f|0;
                                if((f|0)>0)
                                {
                                    Xn(Wa|0,48,(f>>>0>256?256:f)|0)|0;
                                    if(f>>>0>255)
                                    {
                                        q=f;
                                        do
                                        {
                                            Qd(Wa,256,e)|0;
                                            q=q+-256|0
                                        }
                                        while(q>>>0>255);
                                        f=f&255
                                    }
                                    Qd(Wa,f,e)|0
                                }
                                Qd(z,t,e)|0;
                                if((r|0)==8192&w)
                                {
                                    f=M-x|0;
                                    Xn(Wa|0,32,(f>>>0>256?256:f)|0)|0;
                                    if(f>>>0>255)
                                    {
                                        q=f;
                                        do
                                        {
                                            Qd(Wa,256,e)|0;
                                            q=q+-256|0
                                        }
                                        while(q>>>0>255);
                                        f=f&255
                                    }
                                    Qd(Wa,f,e)|0
                                }
                                q=w?M:x;
                                break
                            }
                            f=(C|0)<0?6:C;
                            if(t)
                            {
                                t=(c[Pa>>2]|0)+-28|0;
                                c[Pa>>2]=t;
                                s=y*268435456.0
                            }
                            else
                            {
                                s=y;
                                t=c[Pa>>2]|0
                            }
                            K=(t|0)<0?Ma:va;
                            F=K;
                            u=K;
                            do
                            {
                                E=~~s>>>0;
                                c[u>>2]=E;
                                u=u+4|0;
                                s=(s-+(E>>>0))*1.0e9
                            }
                            while(s!=0.0);
                            t=c[Pa>>2]|0;
                            if((t|0)>0)
                            {
                                q=t;
                                t=K;
                                do
                                {
                                    r=(q|0)>29?29:q;
                                    p=u+-4|0;
                                    do if(p>>>0>=t>>>0)
                                    {
                                        q=0;
                                        do
                                        {
                                            E=$n(c[p>>2]|0,0,r|0)|0;
                                            E=Wn(E|0,G|0,q|0,0)|0;
                                            q=G;
                                            C=jo(E|0,q|0,1e9,0)|0;
                                            c[p>>2]=C;
                                            q=io(E|0,q|0,1e9,0)|0;
                                            p=p+-4|0
                                        }
                                        while(p>>>0>=t>>>0);
                                        if(!q)break;
                                        t=t+-4|0;
                                        c[t>>2]=q
                                    }
                                    while(0);
                                    while(1)
                                    {
                                        if(u>>>0<=t>>>0)break;
                                        q=u+-4|0;
                                        if(!(c[q>>2]|0))u=q;
                                        else break
                                    }
                                    q=(c[Pa>>2]|0)-r|0;
                                    c[Pa>>2]=q
                                }
                                while((q|0)>0)
                            }
                            else
                            {
                                q=t;
                                t=K
                            }
                            g:do if((q|0)<0)
                            {
                                A=((f+25|0)/9|0)+1|0;
                                if((D|0)!=102)while(1)
                                    {
                                    x=0-q|0;
                                    x=(x|0)>9?9:x;
                                    do if(t>>>0<u>>>0)
                                    {
                                        p=(1<<x)+-1|0;
                                        r=1e9>>>x;
                                        q=0;
                                        w=t;
                                        do
                                        {
                                            E=c[w>>2]|0;
                                            c[w>>2]=(E>>>x)+q;
                                            q=ca(E&p,r)|0;
                                            w=w+4|0
                                        }
                                        while(w>>>0<u>>>0);
                                        t=(c[t>>2]|0)==0?t+4|0:t;
                                        if(!q)break;
                                        c[u>>2]=q;
                                        u=u+4|0
                                    }
                                    else t=(c[t>>2]|0)==0?t+4|0:t;
                                    while(0);
                                    u=(u-t>>2|0)>(A|0)?t+(A<<2)|0:u;
                                    q=(c[Pa>>2]|0)+x|0;
                                    c[Pa>>2]=q;
                                    if((q|0)>=0)break g
                                }
                                z=K+(A<<2)|0;
                                do
                                {
                                    x=0-q|0;
                                    x=(x|0)>9?9:x;
                                    do if(t>>>0<u>>>0)
                                    {
                                        p=(1<<x)+-1|0;
                                        r=1e9>>>x;
                                        q=0;
                                        w=t;
                                        do
                                        {
                                            E=c[w>>2]|0;
                                            c[w>>2]=(E>>>x)+q;
                                            q=ca(E&p,r)|0;
                                            w=w+4|0
                                        }
                                        while(w>>>0<u>>>0);
                                        t=(c[t>>2]|0)==0?t+4|0:t;
                                        if(!q)break;
                                        c[u>>2]=q;
                                        u=u+4|0
                                    }
                                    else t=(c[t>>2]|0)==0?t+4|0:t;
                                    while(0);
                                    u=(u-F>>2|0)>(A|0)?z:u;
                                    q=(c[Pa>>2]|0)+x|0;
                                    c[Pa>>2]=q
                                }
                                while((q|0)<0)
                            }
                            while(0);
                            do if(t>>>0<u>>>0)
                            {
                                q=(F-t>>2)*9|0;
                                r=c[t>>2]|0;
                                if(r>>>0<10)
                                {
                                    A=q;
                                    break
                                }
                                else p=10;
                                do
                                {
                                    p=p*10|0;
                                    q=q+1|0
                                }
                                while(r>>>0>=p>>>0);
                                A=q
                            }
                            else A=0;
                            while(0);
                            B=(D|0)==103;
                            C=(f|0)!=0;
                            q=f-((D|0)!=102?A:0)+((C&B)<<31>>31)|0;
                            if((q|0)<(((u-F>>2)*9|0)+-9|0))
                            {
                                r=q+9216|0;
                                x=(r|0)/9|0;
                                q=K+(x+-1023<<2)|0;
                                r=((r|0)%9|0)+1|0;
                                if((r|0)<9)
                                {
                                    p=10;
                                    do
                                    {
                                        p=p*10|0;
                                        r=r+1|0
                                    }
                                    while((r|0)!=9);
                                    z=p
                                }
                                else z=10;
                                r=c[q>>2]|0;
                                w=(r>>>0)%(z>>>0)|0;
                                if((w|0)==0?(K+(x+-1022<<2)|0)==(u|0):0)
                                {
                                    X=t;
                                    W=q;
                                    V=A
                                }
                                else O=221;
                                do if((O|0)==221)
                                {
                                    O=0;
                                    y=(((r>>>0)/(z>>>0)|0)&1|0)==0?9007199254740992.0:9007199254740994.0;
                                    p=(z|0)/2|0;
                                    do if(w>>>0<p>>>0)s=.5;
                                    else
                                    {
                                        if((w|0)==(p|0)?(K+(x+-1022<<2)|0)==(u|0):0)
                                        {
                                            s=1.0;
                                            break
                                        }
                                        s=1.5
                                    }
                                    while(0);
                                    do if(I)
                                    {
                                        if((a[J>>0]|0)!=45)break;
                                        y=-y;
                                        s=-s
                                    }
                                    while(0);
                                    p=r-w|0;
                                    c[q>>2]=p;
                                    if(!(y+s!=y))
                                    {
                                        X=t;
                                        W=q;
                                        V=A;
                                        break
                                    }
                                    W=p+z|0;
                                    c[q>>2]=W;
                                    if(W>>>0>999999999)while(1)
                                        {
                                        p=q+-4|0;
                                        c[q>>2]=0;
                                        if(p>>>0<t>>>0)
                                        {
                                            t=t+-4|0;
                                            c[t>>2]=0
                                        }
                                        W=(c[p>>2]|0)+1|0;
                                        c[p>>2]=W;
                                        if(W>>>0>999999999)q=p;
                                        else
                                        {
                                            q=p;
                                            break
                                        }
                                    }
                                    p=(F-t>>2)*9|0;
                                    w=c[t>>2]|0;
                                    if(w>>>0<10)
                                    {
                                        X=t;
                                        W=q;
                                        V=p;
                                        break
                                    }
                                    else r=10;
                                    do
                                    {
                                        r=r*10|0;
                                        p=p+1|0
                                    }
                                    while(w>>>0>=r>>>0);
                                    X=t;
                                    W=q;
                                    V=p
                                }
                                while(0);
                                E=W+4|0;
                                t=X;
                                A=V;
                                u=u>>>0>E>>>0?E:u
                            }
                            x=0-A|0;
                            while(1)
                            {
                                if(u>>>0<=t>>>0)
                                {
                                    E=0;
                                    break
                                }
                                q=u+-4|0;
                                if(!(c[q>>2]|0))u=q;
                                else
                                {
                                    E=1;
                                    break
                                }
                            }
                            do if(B)
                            {
                                f=(C&1^1)+f|0;
                                if((f|0)>(A|0)&(A|0)>-5)
                                {
                                    w=H+-1|0;
                                    f=f+-1-A|0
                                }
                                else
                                {
                                    w=H+-2|0;
                                    f=f+-1|0
                                }
                                q=L&8;
                                if(q)
                                {
                                    C=q;
                                    break
                                }
                                do if(E)
                                {
                                    q=c[u+-4>>2]|0;
                                    if(!q)
                                    {
                                        p=9;
                                        break
                                    }
                                    if(!((q>>>0)%10|0))
                                    {
                                        r=10;
                                        p=0
                                    }
                                    else
                                    {
                                        p=0;
                                        break
                                    }
                                    do
                                    {
                                        r=r*10|0;
                                        p=p+1|0
                                    }
                                    while(((q>>>0)%(r>>>0)|0|0)==0)
                                }
                                else p=9;
                                while(0);
                                q=((u-F>>2)*9|0)+-9|0;
                                if((w|32|0)==102)
                                {
                                    C=q-p|0;
                                    C=(C|0)<0?0:C;
                                    f=(f|0)<(C|0)?f:C;
                                    C=0;
                                    break
                                }
                                else
                                {
                                    C=q+A-p|0;
                                    C=(C|0)<0?0:C;
                                    f=(f|0)<(C|0)?f:C;
                                    C=0;
                                    break
                                }
                            }
                            else
                            {
                                w=H;
                                C=L&8
                            }
                            while(0);
                            D=f|C;
                            z=(D|0)!=0&1;
                            B=(w|32|0)==102;
                            if(B)
                            {
                                q=(A|0)>0?A:0;
                                A=0
                            }
                            else
                            {
                                p=(A|0)<0?x:A;
                                if((p|0)<0)
                                {
                                    q=Ia;
                                    x=p;
                                    r=((p|0)<0)<<31>>31;
                                    while(1)
                                    {
                                        p=jo(x|0,r|0,10,0)|0;
                                        q=q+-1|0;
                                        a[q>>0]=p|48;
                                        p=io(x|0,r|0,10,0)|0;
                                        if(r>>>0>9|(r|0)==9&x>>>0>4294967295)
                                        {
                                            x=p;
                                            r=G
                                        }
                                        else break
                                    }
                                }
                                else q=Ia;
                                if(p)while(1)
                                    {
                                    q=q+-1|0;
                                    a[q>>0]=(p>>>0)%10|0|48;
                                    if(p>>>0<10)break;
                                    else p=(p>>>0)/10|0
                                }
                                if((Ka-q|0)<2)do
                                    {
                                    q=q+-1|0;
                                    a[q>>0]=48
                                }
                                while((Ka-q|0)<2);
                                a[q+-1>>0]=(A>>31&2)+43;
                                A=q+-2|0;
                                a[A>>0]=w;
                                q=Ka-A|0
                            }
                            H=I+1+f+z+q|0;
                            z=L&73728;
                            F=(M|0)>(H|0);
                            if((z|0)==0&F)
                            {
                                q=M-H|0;
                                Xn(Wa|0,32,(q>>>0>256?256:q)|0)|0;
                                if(q>>>0>255)
                                {
                                    p=q;
                                    do
                                    {
                                        Qd(Wa,256,e)|0;
                                        p=p+-256|0
                                    }
                                    while(p>>>0>255);
                                    q=q&255
                                }
                                Qd(Wa,q,e)|0
                            }
                            Qd(J,I,e)|0;
                            if((z|0)==65536&F)
                            {
                                q=M-H|0;
                                Xn(Wa|0,48,(q>>>0>256?256:q)|0)|0;
                                if(q>>>0>255)
                                {
                                    p=q;
                                    do
                                    {
                                        Qd(Wa,256,e)|0;
                                        p=p+-256|0
                                    }
                                    while(p>>>0>255);
                                    q=q&255
                                }
                                Qd(Wa,q,e)|0
                            }
                            if(B)
                            {
                                r=t>>>0>K>>>0?K:t;
                                q=r;
                                do
                                {
                                    p=c[q>>2]|0;
                                    if(!p)t=wa;
                                    else
                                    {
                                        t=wa;
                                        while(1)
                                        {
                                            t=t+-1|0;
                                            a[t>>0]=(p>>>0)%10|0|48;
                                            if(p>>>0<10)break;
                                            else p=(p>>>0)/10|0
                                        }
                                    }
                                    do if((q|0)==(r|0))
                                    {
                                        if((t|0)!=(wa|0))break;
                                        a[ya>>0]=48;
                                        t=ya
                                    }
                                    else
                                    {
                                        if(t>>>0<=Na>>>0)break;
                                        do
                                        {
                                            t=t+-1|0;
                                            a[t>>0]=48
                                        }
                                        while(t>>>0>Na>>>0)
                                    }
                                    while(0);
                                    Qd(t,xa-t|0,e)|0;
                                    q=q+4|0
                                }
                                while(q>>>0<=K>>>0);
                                if(D)Qd(2480,1,e)|0;
                                if((f|0)>0&q>>>0<u>>>0)
                                {
                                    p=q;
                                    do
                                    {
                                        t=c[p>>2]|0;
                                        if(t)
                                        {
                                            q=wa;
                                            while(1)
                                            {
                                                q=q+-1|0;
                                                a[q>>0]=(t>>>0)%10|0|48;
                                                if(t>>>0<10)break;
                                                else t=(t>>>0)/10|0
                                            }
                                            if(q>>>0>Na>>>0)
                                            {
                                                Za=q;
                                                O=289
                                            }
                                            else ka=q
                                        }
                                        else
                                        {
                                            Za=wa;
                                            O=289
                                        }
                                        if((O|0)==289)while(1)
                                            {
                                            O=0;
                                            q=Za+-1|0;
                                            a[q>>0]=48;
                                            if(q>>>0>Na>>>0)Za=q;
                                            else
                                            {
                                                ka=q;
                                                break
                                            }
                                        }
                                        L=(f|0)>9;
                                        Qd(ka,L?9:f,e)|0;
                                        p=p+4|0;
                                        f=f+-9|0
                                    }
                                    while(L&p>>>0<u>>>0)
                                }
                                if((f|0)>0)
                                {
                                    Xn(Wa|0,48,(f>>>0>256?256:f)|0)|0;
                                    if(f>>>0>255)
                                    {
                                        q=f;
                                        do
                                        {
                                            Qd(Wa,256,e)|0;
                                            q=q+-256|0
                                        }
                                        while(q>>>0>255);
                                        f=f&255
                                    }
                                    Qd(Wa,f,e)|0
                                }
                            }
                            else
                            {
                                x=E?u:t+4|0;
                                do if((f|0)>-1)
                                {
                                    w=(C|0)==0;
                                    r=t;
                                    do
                                    {
                                        u=c[r>>2]|0;
                                        if(u)
                                        {
                                            q=wa;
                                            p=u;
                                            while(1)
                                            {
                                                u=q+-1|0;
                                                a[u>>0]=(p>>>0)%10|0|48;
                                                if(p>>>0<10)break;
                                                else
                                                {
                                                    q=u;
                                                    p=(p>>>0)/10|0
                                                }
                                            }
                                            if((u|0)!=(wa|0))
                                            {
                                                ta=q;
                                                _a=u
                                            }
                                            else O=303
                                        }
                                        else O=303;
                                        if((O|0)==303)
                                        {
                                            O=0;
                                            a[ya>>0]=48;
                                            ta=wa;
                                            _a=ya
                                        }
                                        do if((r|0)==(t|0))
                                        {
                                            Qd(_a,1,e)|0;
                                            if(w&(f|0)<1)
                                            {
                                                u=ta;
                                                break
                                            }
                                            Qd(2480,1,e)|0;
                                            u=ta
                                        }
                                        else
                                        {
                                            if(_a>>>0>Na>>>0)u=_a;
                                            else
                                            {
                                                u=_a;
                                                break
                                            }
                                            do
                                            {
                                                u=u+-1|0;
                                                a[u>>0]=48
                                            }
                                            while(u>>>0>Na>>>0)
                                        }
                                        while(0);
                                        L=xa-u|0;
                                        Qd(u,(f|0)>(L|0)?L:f,e)|0;
                                        f=f-L|0;
                                        r=r+4|0
                                    }
                                    while(r>>>0<x>>>0&(f|0)>-1);
                                    if((f|0)<=0)break;
                                    Xn(Wa|0,48,(f>>>0>256?256:f)|0)|0;
                                    if(f>>>0>255)
                                    {
                                        q=f;
                                        do
                                        {
                                            Qd(Wa,256,e)|0;
                                            q=q+-256|0
                                        }
                                        while(q>>>0>255);
                                        f=f&255
                                    }
                                    Qd(Wa,f,e)|0
                                }
                                while(0);
                                Qd(A,Ka-A|0,e)|0
                            }
                            if((z|0)==8192&F)
                            {
                                f=M-H|0;
                                Xn(Wa|0,32,(f>>>0>256?256:f)|0)|0;
                                if(f>>>0>255)
                                {
                                    q=f;
                                    do
                                    {
                                        Qd(Wa,256,e)|0;
                                        q=q+-256|0
                                    }
                                    while(q>>>0>255);
                                    f=f&255
                                }
                                Qd(Wa,f,e)|0
                            }
                            q=F?M:H
                        }
                        else
                        {
                            p=(H&32|0)!=0;
                            u=s!=s|0.0!=0.0;
                            t=u?0:I;
                            p=u?(p?2464:2472):p?2448:2456;
                            u=t+3|0;
                            r=(M|0)>(u|0);
                            if((L&8192|0)==0&r)
                            {
                                f=M-u|0;
                                Xn(Wa|0,32,(f>>>0>256?256:f)|0)|0;
                                if(f>>>0>255)
                                {
                                    q=f;
                                    do
                                    {
                                        Qd(Wa,256,e)|0;
                                        q=q+-256|0
                                    }
                                    while(q>>>0>255);
                                    f=f&255
                                }
                                Qd(Wa,f,e)|0
                            }
                            Qd(J,t,e)|0;
                            Qd(p,3,e)|0;
                            if((L&73728|0)==8192&r)
                            {
                                f=M-u|0;
                                Xn(Wa|0,32,(f>>>0>256?256:f)|0)|0;
                                if(f>>>0>255)
                                {
                                    q=f;
                                    do
                                    {
                                        Qd(Wa,256,e)|0;
                                        q=q+-256|0
                                    }
                                    while(q>>>0>255);
                                    f=f&255
                                }
                                Qd(Wa,f,e)|0
                            }
                            q=r?M:u
                        }
                        while(0);
                        A=Ba;
                        B=Aa;
                        u=$;
                        f=N;
                        continue a
                    }
                case 115:
                    {
                        La=(Aa|0)!=0?Aa:2416;
                        O=94;
                        break
                    }
                case 67:
                    {
                        c[ab>>2]=Aa;
                        c[Ga>>2]=0;
                        ra=ab;
                        sa=Ha;
                        Va=-1;
                        O=97;
                        break
                    }
                case 83:
                    {
                        f=Aa;
                        if(!C)
                        {
                            Z=Aa;
                            _=f;
                            Y=0;
                            O=102
                        }
                        else
                        {
                            ra=f;
                            sa=Aa;
                            Va=C;
                            O=97
                        }
                        break
                    }
                case 110:switch(x|0)
                    {
                    case 0:
                        {
                            c[Aa>>2]=$;
                            A=Ba;
                            B=Aa;
                            u=$;
                            f=N;
                            continue a
                        }
                    case 1:
                        {
                            c[Aa>>2]=$;
                            A=Ba;
                            B=Aa;
                            u=$;
                            f=N;
                            continue a
                        }
                    case 2:
                        {
                            A=Aa;
                            c[A>>2]=$;
                            c[A+4>>2]=(($|0)<0)<<31>>31;
                            A=Ba;
                            B=Aa;
                            u=$;
                            f=N;
                            continue a
                        }
                    case 3:
                        {
                            b[Aa>>1]=$;
                            A=Ba;
                            B=Aa;
                            u=$;
                            f=N;
                            continue a
                        }
                    case 7:
                        {
                            A=Aa;
                            c[A>>2]=$;
                            c[A+4>>2]=(($|0)<0)<<31>>31;
                            A=Ba;
                            B=Aa;
                            u=$;
                            f=N;
                            continue a
                        }
                    case 6:
                        {
                            c[Aa>>2]=$;
                            A=Ba;
                            B=Aa;
                            u=$;
                            f=N;
                            continue a
                        }
                    case 4:
                        {
                            a[Aa>>0]=$;
                            A=Ba;
                            B=Aa;
                            u=$;
                            f=N;
                            continue a
                        }
                    default:
                        {
                            A=Ba;
                            B=Aa;
                            u=$;
                            f=N;
                            continue a
                        }
                }
                case 105:case 100:
                    {
                        if((Ba|0)<0)
                        {
                            Ea=Vn(0,0,Aa|0,Ba|0)|0;
                            Da=G;
                            Xa=1;
                            Ya=2400;
                            O=84;
                            break f
                        }
                        if(!(L&2048))
                        {
                            Ya=L&1;
                            Da=Ba;
                            Ea=Aa;
                            Xa=Ya;
                            Ya=(Ya|0)==0?2400:2402;
                            O=84
                        }
                        else
                        {
                            Da=Ba;
                            Ea=Aa;
                            Xa=1;
                            Ya=2401;
                            O=84
                        }
                        break
                    }
                case 99:
                    {
                        a[Fa>>0]=Aa;
                        ga=Ba;
                        ha=Aa;
                        ia=Fa;
                        o=t;
                        aa=1;
                        ba=0;
                        da=2400;
                        fa=za;
                        break
                    }
                case 88:case 120:
                    {
                        Qa=L;
                        Ua=C;
                        $a=H;
                        O=73;
                        break
                    }
                case 109:
                    {
                        La=Cb(c[(ob()|0)>>2]|0)|0;
                        O=94;
                        break
                    }
                case 112:
                    {
                        Qa=L|8;
                        Ua=C>>>0>8?C:8;
                        $a=120;
                        O=73;
                        break
                    }
                case 111:
                    {
                        p=(Aa|0)==0&(Ba|0)==0;
                        if(p)n=za;
                        else
                        {
                            n=za;
                            f=Aa;
                            q=Ba;
                            do
                            {
                                n=n+-1|0;
                                a[n>>0]=f&7|48;
                                f=Zn(f|0,q|0,3)|0;
                                q=G
                            }
                            while(!((f|0)==0&(q|0)==0))
                        }
                        S=(L&8|0)==0|p;
                        T=Aa;
                        U=Ba;
                        P=L;
                        Q=C;
                        R=S&1^1;
                        S=S?2400:2405;
                        O=89;
                        break
                    }
                case 117:
                    {
                        Da=Ba;
                        Ea=Aa;
                        Xa=0;
                        Ya=2400;
                        O=84;
                        break
                    }
                default:
                    {
                        ga=Ba;
                        ha=Aa;
                        ia=f;
                        o=L;
                        aa=C;
                        ba=0;
                        da=2400;
                        fa=za
                    }
            }
            while(0);
            if((O|0)==73)
            {
                n=$a&32;
                if(!((Aa|0)==0&(Ba|0)==0))
                {
                    f=za;
                    p=Aa;
                    q=Ba;
                    do
                    {
                        f=f+-1|0;
                        a[f>>0]=d[2384+(p&15)>>0]|n;
                        p=Zn(p|0,q|0,4)|0;
                        q=G
                    }
                    while(!((p|0)==0&(q|0)==0));
                    if(!(Qa&8))
                    {
                        T=Aa;
                        U=Ba;
                        n=f;
                        P=Qa;
                        Q=Ua;
                        R=0;
                        S=2400;
                        O=89
                    }
                    else
                    {
                        T=Aa;
                        U=Ba;
                        n=f;
                        P=Qa;
                        Q=Ua;
                        R=2;
                        S=2400+($a>>4)|0;
                        O=89
                    }
                }
                else
                {
                    T=Aa;
                    U=Ba;
                    n=za;
                    P=Qa;
                    Q=Ua;
                    R=0;
                    S=2400;
                    O=89
                }
            }
            else if((O|0)==84)
            {
                if(Da>>>0>0|(Da|0)==0&Ea>>>0>4294967295)
                {
                    n=za;
                    q=Ea;
                    p=Da;
                    while(1)
                    {
                        f=jo(q|0,p|0,10,0)|0;
                        n=n+-1|0;
                        a[n>>0]=f|48;
                        f=io(q|0,p|0,10,0)|0;
                        if(p>>>0>9|(p|0)==9&q>>>0>4294967295)
                        {
                            q=f;
                            p=G
                        }
                        else break
                    }
                }
                else
                {
                    n=za;
                    f=Ea
                }
                if(!f)
                {
                    T=Ea;
                    U=Da;
                    P=L;
                    Q=C;
                    R=Xa;
                    S=Ya;
                    O=89
                }
                else while(1)
                    {
                    n=n+-1|0;
                    a[n>>0]=(f>>>0)%10|0|48;
                    if(f>>>0<10)
                    {
                        T=Ea;
                        U=Da;
                        P=L;
                        Q=C;
                        R=Xa;
                        S=Ya;
                        O=89;
                        break
                    }
                    else f=(f>>>0)/10|0
                }
            }
            else if((O|0)==94)
            {
                O=0;
                fa=Xd(La,0,C)|0;
                K=(fa|0)==0;
                ga=Ba;
                ha=Aa;
                ia=La;
                o=t;
                aa=K?C:fa-La|0;
                ba=0;
                da=2400;
                fa=K?La+C|0:fa
            }
            else if((O|0)==97)
            {
                q=0;
                f=0;
                r=ra;
                while(1)
                {
                    p=c[r>>2]|0;
                    if(!p)break;
                    f=Ld(Ta,p)|0;
                    if((f|0)<0|f>>>0>(Va-q|0)>>>0)break;
                    q=f+q|0;
                    if(Va>>>0>q>>>0)r=r+4|0;
                    else break
                }
                if((f|0)<0)
                {
                    cb=-1;
                    O=363;
                    break
                }
                else
                {
                    Z=sa;
                    _=ra;
                    Y=q;
                    O=102
                }
            }
            if((O|0)==89)
            {
                O=0;
                o=(Q|0)>-1?P&-65537:P;
                f=(T|0)!=0|(U|0)!=0;
                if(f|(Q|0)!=0)
                {
                    aa=(f&1^1)+(Ca-n)|0;
                    ga=U;
                    ha=T;
                    ia=n;
                    aa=(Q|0)>(aa|0)?Q:aa;
                    ba=R;
                    da=S;
                    fa=za
                }
                else
                {
                    ga=U;
                    ha=T;
                    ia=za;
                    aa=0;
                    ba=R;
                    da=S;
                    fa=za
                }
            }
            else if((O|0)==102)
            {
                O=0;
                t=L&73728;
                r=(M|0)>(Y|0);
                if((t|0)==0&r)
                {
                    f=M-Y|0;
                    Xn(Wa|0,32,(f>>>0>256?256:f)|0)|0;
                    if(f>>>0>255)
                    {
                        q=f;
                        do
                        {
                            Qd(Wa,256,e)|0;
                            q=q+-256|0
                        }
                        while(q>>>0>255);
                        f=f&255
                    }
                    Qd(Wa,f,e)|0
                }
                h:do if(Y)
                {
                    q=0;
                    p=_;
                    while(1)
                    {
                        f=c[p>>2]|0;
                        if(!f)break h;
                        f=Ld(Ta,f)|0;
                        q=f+q|0;
                        if((q|0)>(Y|0))break h;
                        Qd(Ta,f,e)|0;
                        if(q>>>0>=Y>>>0)break;
                        else p=p+4|0
                    }
                }
                while(0);
                if((t|0)==8192&r)
                {
                    f=M-Y|0;
                    Xn(Wa|0,32,(f>>>0>256?256:f)|0)|0;
                    if(f>>>0>255)
                    {
                        q=f;
                        do
                        {
                            Qd(Wa,256,e)|0;
                            q=q+-256|0
                        }
                        while(q>>>0>255);
                        f=f&255
                    }
                    Qd(Wa,f,e)|0
                }
                A=Ba;
                B=Z;
                u=$;
                f=N;
                q=r?M:Y;
                continue
            }
            w=fa-ia|0;
            u=(aa|0)<(w|0)?w:aa;
            r=ba+u|0;
            x=(M|0)<(r|0)?r:M;
            t=o&73728;
            p=(x|0)>(r|0);
            if((t|0)==0&p)
            {
                f=x-r|0;
                Xn(Wa|0,32,(f>>>0>256?256:f)|0)|0;
                if(f>>>0>255)
                {
                    q=f;
                    do
                    {
                        Qd(Wa,256,e)|0;
                        q=q+-256|0
                    }
                    while(q>>>0>255);
                    f=f&255
                }
                Qd(Wa,f,e)|0
            }
            Qd(da,ba,e)|0;
            if((t|0)==65536&p)
            {
                f=x-r|0;
                Xn(Wa|0,48,(f>>>0>256?256:f)|0)|0;
                if(f>>>0>255)
                {
                    q=f;
                    do
                    {
                        Qd(Wa,256,e)|0;
                        q=q+-256|0
                    }
                    while(q>>>0>255);
                    f=f&255
                }
                Qd(Wa,f,e)|0
            }
            if((u|0)>(w|0))
            {
                f=u-w|0;
                Xn(Wa|0,48,(f>>>0>256?256:f)|0)|0;
                if(f>>>0>255)
                {
                    q=f;
                    do
                    {
                        Qd(Wa,256,e)|0;
                        q=q+-256|0
                    }
                    while(q>>>0>255);
                    f=f&255
                }
                Qd(Wa,f,e)|0
            }
            Qd(ia,w,e)|0;
            if((t|0)==8192&p)
            {
                f=x-r|0;
                Xn(Wa|0,32,(f>>>0>256?256:f)|0)|0;
                if(f>>>0>255)
                {
                    q=f;
                    do
                    {
                        Qd(Wa,256,e)|0;
                        q=q+-256|0
                    }
                    while(q>>>0>255);
                    f=f&255
                }
                Qd(Wa,f,e)|0
            }
            A=ga;
            B=ha;
            u=$;
            f=N;
            q=x
        }
        if((O|0)==344)
        {
            if(e)
            {
                l=Oa;
                i=db;
                return l|0
            }
            if(!Sa)
            {
                l=0;
                i=db;
                return l|0
            }
            else f=1;
            while(1)
            {
                n=c[l+(f<<2)>>2]|0;
                if(!n)
                {
                    m=f;
                    break
                }
                o=j+(f<<3)|0;
                i:do if(n>>>0<=20)do switch(n|0)
                    {
                    case 9:
                        {
                            ab=(c[g>>2]|0)+(4-1)&~(4-1);
                            bb=c[ab>>2]|0;
                            c[g>>2]=ab+4;
                            c[o>>2]=bb;
                            break i
                        }
                    case 15:
                        {
                            bb=(c[g>>2]|0)+(4-1)&~(4-1);
                            ab=c[bb>>2]|0;
                            c[g>>2]=bb+4;
                            ab=(ab&255)<<24>>24;
                            bb=o;
                            c[bb>>2]=ab;
                            c[bb+4>>2]=((ab|0)<0)<<31>>31;
                            break i
                        }
                    case 10:
                        {
                            bb=(c[g>>2]|0)+(4-1)&~(4-1);
                            ab=c[bb>>2]|0;
                            c[g>>2]=bb+4;
                            bb=o;
                            c[bb>>2]=ab;
                            c[bb+4>>2]=((ab|0)<0)<<31>>31;
                            break i
                        }
                    case 12:
                        {
                            bb=(c[g>>2]|0)+(8-1)&~(8-1);
                            ab=bb;
                            $a=c[ab>>2]|0;
                            ab=c[ab+4>>2]|0;
                            c[g>>2]=bb+8;
                            bb=o;
                            c[bb>>2]=$a;
                            c[bb+4>>2]=ab;
                            break i
                        }
                    case 11:
                        {
                            bb=(c[g>>2]|0)+(4-1)&~(4-1);
                            ab=c[bb>>2]|0;
                            c[g>>2]=bb+4;
                            bb=o;
                            c[bb>>2]=ab;
                            c[bb+4>>2]=0;
                            break i
                        }
                    case 13:
                        {
                            bb=(c[g>>2]|0)+(4-1)&~(4-1);
                            ab=c[bb>>2]|0;
                            c[g>>2]=bb+4;
                            ab=(ab&65535)<<16>>16;
                            bb=o;
                            c[bb>>2]=ab;
                            c[bb+4>>2]=((ab|0)<0)<<31>>31;
                            break i
                        }
                    case 18:
                        {
                            bb=(c[g>>2]|0)+(8-1)&~(8-1);
                            y=+h[bb>>3];
                            c[g>>2]=bb+8;
                            h[o>>3]=y;
                            break i
                        }
                    case 17:
                        {
                            bb=(c[g>>2]|0)+(8-1)&~(8-1);
                            y=+h[bb>>3];
                            c[g>>2]=bb+8;
                            h[o>>3]=y;
                            break i
                        }
                    case 14:
                        {
                            bb=(c[g>>2]|0)+(4-1)&~(4-1);
                            ab=c[bb>>2]|0;
                            c[g>>2]=bb+4;
                            bb=o;
                            c[bb>>2]=ab&65535;
                            c[bb+4>>2]=0;
                            break i
                        }
                    case 16:
                        {
                            bb=(c[g>>2]|0)+(4-1)&~(4-1);
                            ab=c[bb>>2]|0;
                            c[g>>2]=bb+4;
                            bb=o;
                            c[bb>>2]=ab&255;
                            c[bb+4>>2]=0;
                            break i
                        }
                    default:break i
                }
                while(0);
                while(0);
                f=f+1|0;
                if((f|0)>=10)
                {
                    cb=1;
                    O=363;
                    break
                }
            }
            if((O|0)==363)
            {
                i=db;
                return cb|0
            }
            if((m|0)>=10)
            {
                l=1;
                i=db;
                return l|0
            }
            while(1)
            {
                if(c[l+(m<<2)>>2]|0)
                {
                    cb=-1;
                    O=363;
                    break
                }
                m=m+1|0;
                if((m|0)>=10)
                {
                    cb=1;
                    O=363;
                    break
                }
            }
            if((O|0)==363)
            {
                i=db;
                return cb|0
            }
        }
        else if((O|0)==363)
        {
            i=db;
            return cb|0
        }
        return 0
    }
    function _d(a,b,d)
    {
        a=a|0;
        b=b|0;
        d=d|0;
        var e=0,f=0;
        e=a+20|0;
        f=c[e>>2]|0;
        a=(c[a+16>>2]|0)-f|0;
        a=a>>>0>d>>>0?d:a;
        _n(f|0,b|0,a|0)|0;
        c[e>>2]=(c[e>>2]|0)+a;
        return d|0
    }
    function $d(a)
    {
        a=a|0;
        var b=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0;
        do if(a>>>0<245)
        {
            o=a>>>0<11?16:a+11&-8;
            a=o>>>3;
            k=c[622]|0;
            f=k>>>a;
            if(f&3)
            {
                d=(f&1^1)+a|0;
                f=d<<1;
                e=2528+(f<<2)|0;
                f=2528+(f+2<<2)|0;
                g=c[f>>2]|0;
                h=g+8|0;
                i=c[h>>2]|0;
                do if((e|0)!=(i|0))
                {
                    if(i>>>0<(c[626]|0)>>>0)xb();
                    b=i+12|0;
                    if((c[b>>2]|0)==(g|0))
                    {
                        c[b>>2]=e;
                        c[f>>2]=i;
                        break
                    }
                    else xb()
                }
                else c[622]=k&~(1<<d);
                while(0);
                L=d<<3;
                c[g+4>>2]=L|3;
                L=g+(L|4)|0;
                c[L>>2]=c[L>>2]|1;
                L=h;
                return L|0
            }
            i=c[624]|0;
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
                    d=2528+(e<<2)|0;
                    e=2528+(e+2<<2)|0;
                    g=c[e>>2]|0;
                    j=g+8|0;
                    h=c[j>>2]|0;
                    do if((d|0)!=(h|0))
                    {
                        if(h>>>0<(c[626]|0)>>>0)xb();
                        b=h+12|0;
                        if((c[b>>2]|0)==(g|0))
                        {
                            c[b>>2]=d;
                            c[e>>2]=h;
                            l=c[624]|0;
                            break
                        }
                        else xb()
                    }
                    else
                    {
                        c[622]=k&~(1<<f);
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
                        h=c[627]|0;
                        d=l>>>3;
                        b=d<<1;
                        e=2528+(b<<2)|0;
                        f=c[622]|0;
                        d=1<<d;
                        if(f&d)
                        {
                            f=2528+(b+2<<2)|0;
                            b=c[f>>2]|0;
                            if(b>>>0<(c[626]|0)>>>0)xb();
                            else
                            {
                                m=f;
                                n=b
                            }
                        }
                        else
                        {
                            c[622]=f|d;
                            m=2528+(b+2<<2)|0;
                            n=e
                        }
                        c[m>>2]=h;
                        c[n+12>>2]=h;
                        c[h+8>>2]=n;
                        c[h+12>>2]=e
                    }
                    c[624]=i;
                    c[627]=a;
                    L=j;
                    return L|0
                }
                a=c[623]|0;
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
                    e=c[2792+((H|K|L|f|e)+(d>>>e)<<2)>>2]|0;
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
                    a=c[626]|0;
                    if(e>>>0<a>>>0)xb();
                    i=e+o|0;
                    if(e>>>0>=i>>>0)xb();
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
                        if(f>>>0<a>>>0)xb();
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
                        if(h>>>0<a>>>0)xb();
                        b=h+12|0;
                        if((c[b>>2]|0)!=(e|0))xb();
                        f=d+8|0;
                        if((c[f>>2]|0)==(e|0))
                        {
                            c[b>>2]=d;
                            c[f>>2]=h;
                            g=d;
                            break
                        }
                        else xb()
                    }
                    while(0);
                    do if(j)
                    {
                        b=c[e+28>>2]|0;
                        f=2792+(b<<2)|0;
                        if((e|0)==(c[f>>2]|0))
                        {
                            c[f>>2]=g;
                            if(!g)
                            {
                                c[623]=c[623]&~(1<<b);
                                break
                            }
                        }
                        else
                        {
                            if(j>>>0<(c[626]|0)>>>0)xb();
                            b=j+16|0;
                            if((c[b>>2]|0)==(e|0))c[b>>2]=g;
                            else c[j+20>>2]=g;
                            if(!g)break
                        }
                        f=c[626]|0;
                        if(g>>>0<f>>>0)xb();
                        c[g+24>>2]=j;
                        b=c[e+16>>2]|0;
                        do if(b)if(b>>>0<f>>>0)xb();
                        else
                        {
                            c[g+16>>2]=b;
                            c[b+24>>2]=g;
                            break
                        }
                        while(0);
                        b=c[e+20>>2]|0;
                        if(b)if(b>>>0<(c[626]|0)>>>0)xb();
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
                        b=c[624]|0;
                        if(b)
                        {
                            g=c[627]|0;
                            d=b>>>3;
                            b=d<<1;
                            h=2528+(b<<2)|0;
                            f=c[622]|0;
                            d=1<<d;
                            if(f&d)
                            {
                                b=2528+(b+2<<2)|0;
                                f=c[b>>2]|0;
                                if(f>>>0<(c[626]|0)>>>0)xb();
                                else
                                {
                                    p=b;
                                    q=f
                                }
                            }
                            else
                            {
                                c[622]=f|d;
                                p=2528+(b+2<<2)|0;
                                q=h
                            }
                            c[p>>2]=g;
                            c[q+12>>2]=g;
                            c[g+8>>2]=q;
                            c[g+12>>2]=h
                        }
                        c[624]=k;
                        c[627]=i
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
            l=c[623]|0;
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
                a=c[2792+(k<<2)>>2]|0;
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
                        d=c[2792+((l|n|p|q|d)+(a>>>d)<<2)>>2]|0;
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
                if((i|0)!=0?g>>>0<((c[624]|0)-m|0)>>>0:0)
                {
                    a=c[626]|0;
                    if(i>>>0<a>>>0)xb();
                    h=i+m|0;
                    if(i>>>0>=h>>>0)xb();
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
                        if(f>>>0<a>>>0)xb();
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
                        if(e>>>0<a>>>0)xb();
                        b=e+12|0;
                        if((c[b>>2]|0)!=(i|0))xb();
                        f=d+8|0;
                        if((c[f>>2]|0)==(i|0))
                        {
                            c[b>>2]=d;
                            c[f>>2]=e;
                            o=d;
                            break
                        }
                        else xb()
                    }
                    while(0);
                    do if(j)
                    {
                        b=c[i+28>>2]|0;
                        f=2792+(b<<2)|0;
                        if((i|0)==(c[f>>2]|0))
                        {
                            c[f>>2]=o;
                            if(!o)
                            {
                                c[623]=c[623]&~(1<<b);
                                break
                            }
                        }
                        else
                        {
                            if(j>>>0<(c[626]|0)>>>0)xb();
                            b=j+16|0;
                            if((c[b>>2]|0)==(i|0))c[b>>2]=o;
                            else c[j+20>>2]=o;
                            if(!o)break
                        }
                        f=c[626]|0;
                        if(o>>>0<f>>>0)xb();
                        c[o+24>>2]=j;
                        b=c[i+16>>2]|0;
                        do if(b)if(b>>>0<f>>>0)xb();
                        else
                        {
                            c[o+16>>2]=b;
                            c[b+24>>2]=o;
                            break
                        }
                        while(0);
                        b=c[i+20>>2]|0;
                        if(b)if(b>>>0<(c[626]|0)>>>0)xb();
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
                            e=2528+(f<<2)|0;
                            d=c[622]|0;
                            b=1<<b;
                            if(d&b)
                            {
                                b=2528+(f+2<<2)|0;
                                f=c[b>>2]|0;
                                if(f>>>0<(c[626]|0)>>>0)xb();
                                else
                                {
                                    s=b;
                                    t=f
                                }
                            }
                            else
                            {
                                c[622]=d|b;
                                s=2528+(f+2<<2)|0;
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
                        b=2792+(e<<2)|0;
                        c[i+(m+28)>>2]=e;
                        c[i+(m+20)>>2]=0;
                        c[i+(m+16)>>2]=0;
                        f=c[623]|0;
                        d=1<<e;
                        if(!(f&d))
                        {
                            c[623]=f|d;
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
                            if(d>>>0<(c[626]|0)>>>0)xb();
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
                        L=c[626]|0;
                        if(d>>>0>=L>>>0&y>>>0>=L>>>0)
                        {
                            c[d+12>>2]=h;
                            c[b>>2]=h;
                            c[i+(m+8)>>2]=d;
                            c[i+(m+12)>>2]=y;
                            c[i+(m+24)>>2]=0;
                            break
                        }
                        else xb()
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
        a=c[624]|0;
        if(a>>>0>=q>>>0)
        {
            b=a-q|0;
            d=c[627]|0;
            if(b>>>0>15)
            {
                c[627]=d+q;
                c[624]=b;
                c[d+(q+4)>>2]=b|1;
                c[d+a>>2]=b;
                c[d+4>>2]=q|3
            }
            else
            {
                c[624]=0;
                c[627]=0;
                c[d+4>>2]=a|3;
                L=d+(a+4)|0;
                c[L>>2]=c[L>>2]|1
            }
            L=d+8|0;
            return L|0
        }
        a=c[625]|0;
        if(a>>>0>q>>>0)
        {
            K=a-q|0;
            c[625]=K;
            L=c[628]|0;
            c[628]=L+q;
            c[L+(q+4)>>2]=K|1;
            c[L+4>>2]=q|3;
            L=L+8|0;
            return L|0
        }
        do if(!(c[740]|0))
        {
            a=vb(30)|0;
            if(!(a+-1&a))
            {
                c[742]=a;
                c[741]=a;
                c[743]=-1;
                c[744]=-1;
                c[745]=0;
                c[733]=0;
                c[740]=(Ab(0)|0)&-16^1431655768;
                break
            }
            else xb()
        }
        while(0);
        i=q+48|0;
        g=c[742]|0;
        k=q+47|0;
        h=g+k|0;
        g=0-g|0;
        l=h&g;
        if(l>>>0<=q>>>0)
        {
            L=0;
            return L|0
        }
        a=c[732]|0;
        if((a|0)!=0?(t=c[730]|0,y=t+l|0,y>>>0<=t>>>0|y>>>0>a>>>0):0)
        {
            L=0;
            return L|0
        }
        d:do if(!(c[733]&4))
        {
            a=c[628]|0;
            e:do if(a)
            {
                d=2936;
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
                f=h-(c[625]|0)&g;
                if(f>>>0<2147483647)
                {
                    d=Ua(f|0)|0;
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
                j=Ua(0)|0;
                if((j|0)!=(-1|0))
                {
                    a=j;
                    f=c[741]|0;
                    d=f+-1|0;
                    if(!(d&a))f=l;
                    else f=l-a+(d+a&0-f)|0;
                    a=c[730]|0;
                    d=a+f|0;
                    if(f>>>0>q>>>0&f>>>0<2147483647)
                    {
                        y=c[732]|0;
                        if((y|0)!=0?d>>>0<=a>>>0|d>>>0>y>>>0:0)
                        {
                            a=0;
                            break
                        }
                        d=Ua(f|0)|0;
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
                do if(i>>>0>f>>>0&(f>>>0<2147483647&(d|0)!=(-1|0))?(u=c[742]|0,u=k-f+u&0-u,u>>>0<2147483647):0)if((Ua(u|0)|0)==(-1|0))
                {
                    Ua(j|0)|0;
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
            c[733]=c[733]|4;
            v=191
        }
        else
        {
            a=0;
            v=191
        }
        while(0);
        if((((v|0)==191?l>>>0<2147483647:0)?(w=Ua(l|0)|0,x=Ua(0)|0,w>>>0<x>>>0&((w|0)!=(-1|0)&(x|0)!=(-1|0))):0)?(z=x-w|0,A=z>>>0>(q+40|0)>>>0,A):0)
        {
            p=A?z:a;
            v=194
        }
        if((v|0)==194)
        {
            a=(c[730]|0)+p|0;
            c[730]=a;
            if(a>>>0>(c[731]|0)>>>0)c[731]=a;
            g=c[628]|0;
            g:do if(g)
            {
                j=2936;
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
                    L=(c[625]|0)+p|0;
                    K=g+8|0;
                    K=(K&7|0)==0?0:0-K&7;
                    H=L-K|0;
                    c[628]=g+K;
                    c[625]=H;
                    c[g+(K+4)>>2]=H|1;
                    c[g+(L+4)>>2]=40;
                    c[629]=c[744];
                    break
                }
                a=c[626]|0;
                if(w>>>0<a>>>0)
                {
                    c[626]=w;
                    a=w
                }
                f=w+p|0;
                h=2936;
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
                        d=2936;
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
                        if((b|0)==(c[627]|0))
                        {
                            L=(c[624]|0)+l|0;
                            c[624]=L;
                            c[627]=o;
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
                                    if(d>>>0<a>>>0)xb();
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
                                    if(e>>>0<a>>>0)xb();
                                    a=e+12|0;
                                    if((c[a>>2]|0)!=(b|0))xb();
                                    f=d+8|0;
                                    if((c[f>>2]|0)==(b|0))
                                    {
                                        c[a>>2]=d;
                                        c[f>>2]=e;
                                        J=d;
                                        break
                                    }
                                    else xb()
                                }
                                while(0);
                                if(!j)break;
                                a=c[w+(p+28+k)>>2]|0;
                                f=2792+(a<<2)|0;
                                do if((b|0)!=(c[f>>2]|0))
                                {
                                    if(j>>>0<(c[626]|0)>>>0)xb();
                                    a=j+16|0;
                                    if((c[a>>2]|0)==(b|0))c[a>>2]=J;
                                    else c[j+20>>2]=J;
                                    if(!J)break i
                                }
                                else
                                {
                                    c[f>>2]=J;
                                    if(J)break;
                                    c[623]=c[623]&~(1<<a);
                                    break i
                                }
                                while(0);
                                f=c[626]|0;
                                if(J>>>0<f>>>0)xb();
                                c[J+24>>2]=j;
                                b=k|16;
                                a=c[w+(b+p)>>2]|0;
                                do if(a)if(a>>>0<f>>>0)xb();
                                else
                                {
                                    c[J+16>>2]=a;
                                    c[a+24>>2]=J;
                                    break
                                }
                                while(0);
                                b=c[w+(g+b)>>2]|0;
                                if(!b)break;
                                if(b>>>0<(c[626]|0)>>>0)xb();
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
                                f=2528+(h<<1<<2)|0;
                                do if((d|0)!=(f|0))
                                {
                                    if(d>>>0<a>>>0)xb();
                                    if((c[d+12>>2]|0)==(b|0))break;
                                    xb()
                                }
                                while(0);
                                if((e|0)==(d|0))
                                {
                                    c[622]=c[622]&~(1<<h);
                                    break
                                }
                                do if((e|0)==(f|0))F=e+8|0;
                                else
                                {
                                    if(e>>>0<a>>>0)xb();
                                    a=e+8|0;
                                    if((c[a>>2]|0)==(b|0))
                                    {
                                        F=a;
                                        break
                                    }
                                    xb()
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
                            e=2528+(f<<2)|0;
                            d=c[622]|0;
                            b=1<<b;
                            do if(!(d&b))
                            {
                                c[622]=d|b;
                                K=2528+(f+2<<2)|0;
                                L=e
                            }
                            else
                            {
                                b=2528+(f+2<<2)|0;
                                f=c[b>>2]|0;
                                if(f>>>0>=(c[626]|0)>>>0)
                                {
                                    K=b;
                                    L=f;
                                    break
                                }
                                xb()
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
                        b=2792+(e<<2)|0;
                        c[w+(m+28)>>2]=e;
                        c[w+(m+20)>>2]=0;
                        c[w+(m+16)>>2]=0;
                        f=c[623]|0;
                        d=1<<e;
                        if(!(f&d))
                        {
                            c[623]=f|d;
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
                            if(d>>>0<(c[626]|0)>>>0)xb();
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
                        L=c[626]|0;
                        if(d>>>0>=L>>>0&M>>>0>=L>>>0)
                        {
                            c[d+12>>2]=o;
                            c[b>>2]=o;
                            c[w+(m+8)>>2]=d;
                            c[w+(m+12)>>2]=M;
                            c[w+(m+24)>>2]=0;
                            break
                        }
                        else xb()
                    }
                    else
                    {
                        L=(c[625]|0)+l|0;
                        c[625]=L;
                        c[628]=o;
                        c[w+(m+4)>>2]=L|1
                    }
                    while(0);
                    L=w+(n|8)|0;
                    return L|0
                }
                else d=2936;
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
                c[628]=w+d;
                c[625]=L;
                c[w+(d+4)>>2]=L|1;
                c[w+(p+-36)>>2]=40;
                c[629]=c[744];
                d=f+4|0;
                c[d>>2]=27;
                c[b>>2]=c[734];
                c[b+4>>2]=c[735];
                c[b+8>>2]=c[736];
                c[b+12>>2]=c[737];
                c[734]=w;
                c[735]=p;
                c[737]=0;
                c[736]=b;
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
                        e=2528+(f<<2)|0;
                        d=c[622]|0;
                        b=1<<b;
                        if(d&b)
                        {
                            b=2528+(f+2<<2)|0;
                            d=c[b>>2]|0;
                            if(d>>>0<(c[626]|0)>>>0)xb();
                            else
                            {
                                G=b;
                                H=d
                            }
                        }
                        else
                        {
                            c[622]=d|b;
                            G=2528+(f+2<<2)|0;
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
                    f=2792+(e<<2)|0;
                    c[g+28>>2]=e;
                    c[g+20>>2]=0;
                    c[a>>2]=0;
                    b=c[623]|0;
                    d=1<<e;
                    if(!(b&d))
                    {
                        c[623]=b|d;
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
                        if(d>>>0<(c[626]|0)>>>0)xb();
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
                    L=c[626]|0;
                    if(d>>>0>=L>>>0&I>>>0>=L>>>0)
                    {
                        c[d+12>>2]=g;
                        c[b>>2]=g;
                        c[g+8>>2]=d;
                        c[g+12>>2]=I;
                        c[g+24>>2]=0;
                        break
                    }
                    else xb()
                }
            }
            else
            {
                L=c[626]|0;
                if((L|0)==0|w>>>0<L>>>0)c[626]=w;
                c[734]=w;
                c[735]=p;
                c[737]=0;
                c[631]=c[740];
                c[630]=-1;
                b=0;
                do
                {
                    L=b<<1;
                    K=2528+(L<<2)|0;
                    c[2528+(L+3<<2)>>2]=K;
                    c[2528+(L+2<<2)>>2]=K;
                    b=b+1|0
                }
                while((b|0)!=32);
                L=w+8|0;
                L=(L&7|0)==0?0:0-L&7;
                K=p+-40-L|0;
                c[628]=w+L;
                c[625]=K;
                c[w+(L+4)>>2]=K|1;
                c[w+(p+-36)>>2]=40;
                c[629]=c[744]
            }
            while(0);
            b=c[625]|0;
            if(b>>>0>q>>>0)
            {
                K=b-q|0;
                c[625]=K;
                L=c[628]|0;
                c[628]=L+q;
                c[L+(q+4)>>2]=K|1;
                c[L+4>>2]=q|3;
                L=L+8|0;
                return L|0
            }
        }
        c[(ob()|0)>>2]=12;
        L=0;
        return L|0
    }
    function ae(a)
    {
        a=a|0;
        var b=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;
        if(!a)return;
        b=a+-8|0;
        i=c[626]|0;
        if(b>>>0<i>>>0)xb();
        f=c[a+-4>>2]|0;
        d=f&3;
        if((d|0)==1)xb();
        o=f&-8;
        q=a+(o+-8)|0;
        do if(!(f&1))
        {
            b=c[b>>2]|0;
            if(!d)return;
            j=-8-b|0;
            l=a+j|0;
            m=b+o|0;
            if(l>>>0<i>>>0)xb();
            if((l|0)==(c[627]|0))
            {
                b=a+(o+-4)|0;
                f=c[b>>2]|0;
                if((f&3|0)!=3)
                {
                    u=l;
                    g=m;
                    break
                }
                c[624]=m;
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
                b=2528+(e<<1<<2)|0;
                if((d|0)!=(b|0))
                {
                    if(d>>>0<i>>>0)xb();
                    if((c[d+12>>2]|0)!=(l|0))xb()
                }
                if((f|0)==(d|0))
                {
                    c[622]=c[622]&~(1<<e);
                    u=l;
                    g=m;
                    break
                }
                if((f|0)!=(b|0))
                {
                    if(f>>>0<i>>>0)xb();
                    b=f+8|0;
                    if((c[b>>2]|0)==(l|0))h=b;
                    else xb()
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
                if(f>>>0<i>>>0)xb();
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
                if(e>>>0<i>>>0)xb();
                b=e+12|0;
                if((c[b>>2]|0)!=(l|0))xb();
                f=d+8|0;
                if((c[f>>2]|0)==(l|0))
                {
                    c[b>>2]=d;
                    c[f>>2]=e;
                    k=d;
                    break
                }
                else xb()
            }
            while(0);
            if(h)
            {
                b=c[a+(j+28)>>2]|0;
                f=2792+(b<<2)|0;
                if((l|0)==(c[f>>2]|0))
                {
                    c[f>>2]=k;
                    if(!k)
                    {
                        c[623]=c[623]&~(1<<b);
                        u=l;
                        g=m;
                        break
                    }
                }
                else
                {
                    if(h>>>0<(c[626]|0)>>>0)xb();
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
                f=c[626]|0;
                if(k>>>0<f>>>0)xb();
                c[k+24>>2]=h;
                b=c[a+(j+16)>>2]|0;
                do if(b)if(b>>>0<f>>>0)xb();
                else
                {
                    c[k+16>>2]=b;
                    c[b+24>>2]=k;
                    break
                }
                while(0);
                b=c[a+(j+20)>>2]|0;
                if(b)if(b>>>0<(c[626]|0)>>>0)xb();
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
        if(u>>>0>=q>>>0)xb();
        b=a+(o+-4)|0;
        f=c[b>>2]|0;
        if(!(f&1))xb();
        if(!(f&2))
        {
            if((q|0)==(c[628]|0))
            {
                t=(c[625]|0)+g|0;
                c[625]=t;
                c[628]=u;
                c[u+4>>2]=t|1;
                if((u|0)!=(c[627]|0))return;
                c[627]=0;
                c[624]=0;
                return
            }
            if((q|0)==(c[627]|0))
            {
                t=(c[624]|0)+g|0;
                c[624]=t;
                c[627]=u;
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
                    if(f>>>0<(c[626]|0)>>>0)xb();
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
                    if(f>>>0<(c[626]|0)>>>0)xb();
                    d=f+12|0;
                    if((c[d>>2]|0)!=(q|0))xb();
                    e=b+8|0;
                    if((c[e>>2]|0)==(q|0))
                    {
                        c[d>>2]=b;
                        c[e>>2]=f;
                        p=b;
                        break
                    }
                    else xb()
                }
                while(0);
                if(h)
                {
                    b=c[a+(o+20)>>2]|0;
                    f=2792+(b<<2)|0;
                    if((q|0)==(c[f>>2]|0))
                    {
                        c[f>>2]=p;
                        if(!p)
                        {
                            c[623]=c[623]&~(1<<b);
                            break
                        }
                    }
                    else
                    {
                        if(h>>>0<(c[626]|0)>>>0)xb();
                        b=h+16|0;
                        if((c[b>>2]|0)==(q|0))c[b>>2]=p;
                        else c[h+20>>2]=p;
                        if(!p)break
                    }
                    f=c[626]|0;
                    if(p>>>0<f>>>0)xb();
                    c[p+24>>2]=h;
                    b=c[a+(o+8)>>2]|0;
                    do if(b)if(b>>>0<f>>>0)xb();
                    else
                    {
                        c[p+16>>2]=b;
                        c[b+24>>2]=p;
                        break
                    }
                    while(0);
                    b=c[a+(o+12)>>2]|0;
                    if(b)if(b>>>0<(c[626]|0)>>>0)xb();
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
                b=2528+(e<<1<<2)|0;
                if((d|0)!=(b|0))
                {
                    if(d>>>0<(c[626]|0)>>>0)xb();
                    if((c[d+12>>2]|0)!=(q|0))xb()
                }
                if((f|0)==(d|0))
                {
                    c[622]=c[622]&~(1<<e);
                    break
                }
                if((f|0)!=(b|0))
                {
                    if(f>>>0<(c[626]|0)>>>0)xb();
                    b=f+8|0;
                    if((c[b>>2]|0)==(q|0))n=b;
                    else xb()
                }
                else n=f+8|0;
                c[d+12>>2]=f;
                c[n>>2]=d
            }
            while(0);
            c[u+4>>2]=g|1;
            c[u+g>>2]=g;
            if((u|0)==(c[627]|0))
            {
                c[624]=g;
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
            f=2528+(d<<2)|0;
            e=c[622]|0;
            b=1<<b;
            if(e&b)
            {
                b=2528+(d+2<<2)|0;
                d=c[b>>2]|0;
                if(d>>>0<(c[626]|0)>>>0)xb();
                else
                {
                    r=b;
                    s=d
                }
            }
            else
            {
                c[622]=e|b;
                r=2528+(d+2<<2)|0;
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
        b=2792+(f<<2)|0;
        c[u+28>>2]=f;
        c[u+20>>2]=0;
        c[u+16>>2]=0;
        d=c[623]|0;
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
                if(d>>>0<(c[626]|0)>>>0)xb();
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
            s=c[626]|0;
            if(d>>>0>=s>>>0&t>>>0>=s>>>0)
            {
                c[d+12>>2]=u;
                c[b>>2]=u;
                c[u+8>>2]=d;
                c[u+12>>2]=t;
                c[u+24>>2]=0;
                break
            }
            else xb()
        }
        else
        {
            c[623]=d|e;
            c[b>>2]=u;
            c[u+24>>2]=b;
            c[u+12>>2]=u;
            c[u+8>>2]=u
        }
        while(0);
        u=(c[630]|0)+-1|0;
        c[630]=u;
        if(!u)b=2944;
        else return;
        while(1)
        {
            b=c[b>>2]|0;
            if(!b)break;
            else b=b+8|0
        }
        c[630]=-1;
        return
    }
    function be(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0;
        if(!a)
        {
            a=$d(b)|0;
            return a|0
        }
        if(b>>>0>4294967231)
        {
            c[(ob()|0)>>2]=12;
            a=0;
            return a|0
        }
        d=ce(a+-8|0,b>>>0<11?16:b+11&-8)|0;
        if(d)
        {
            a=d+8|0;
            return a|0
        }
        d=$d(b)|0;
        if(!d)
        {
            a=0;
            return a|0
        }
        e=c[a+-4>>2]|0;
        e=(e&-8)-((e&3|0)==0?8:4)|0;
        _n(d|0,a|0,(e>>>0<b>>>0?e:b)|0)|0;
        ae(a);
        a=d;
        return a|0
    }
    function ce(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0;
        o=a+4|0;
        p=c[o>>2]|0;
        j=p&-8;
        l=a+j|0;
        i=c[626]|0;
        d=p&3;
        if(!((d|0)!=1&a>>>0>=i>>>0&a>>>0<l>>>0))xb();
        e=a+(j|4)|0;
        g=c[e>>2]|0;
        if(!(g&1))xb();
        if(!d)
        {
            if(b>>>0<256)
            {
                a=0;
                return a|0
            }
            if(j>>>0>=(b+4|0)>>>0?(j-b|0)>>>0<=c[742]<<1>>>0:0)return a|0;
            a=0;
            return a|0
        }
        if(j>>>0>=b>>>0)
        {
            d=j-b|0;
            if(d>>>0<=15)return a|0;
            c[o>>2]=p&1|b|2;
            c[a+(b+4)>>2]=d|3;
            c[e>>2]=c[e>>2]|1;
            de(a+b|0,d);
            return a|0
        }
        if((l|0)==(c[628]|0))
        {
            d=(c[625]|0)+j|0;
            if(d>>>0<=b>>>0)
            {
                a=0;
                return a|0
            }
            n=d-b|0;
            c[o>>2]=p&1|b|2;
            c[a+(b+4)>>2]=n|1;
            c[628]=a+b;
            c[625]=n;
            return a|0
        }
        if((l|0)==(c[627]|0))
        {
            e=(c[624]|0)+j|0;
            if(e>>>0<b>>>0)
            {
                a=0;
                return a|0
            }
            d=e-b|0;
            if(d>>>0>15)
            {
                c[o>>2]=p&1|b|2;
                c[a+(b+4)>>2]=d|1;
                c[a+e>>2]=d;
                e=a+(e+4)|0;
                c[e>>2]=c[e>>2]&-2;
                e=a+b|0
            }
            else
            {
                c[o>>2]=p&1|e|2;
                e=a+(e+4)|0;
                c[e>>2]=c[e>>2]|1;
                e=0;
                d=0
            }
            c[624]=d;
            c[627]=e;
            return a|0
        }
        if(g&2)
        {
            a=0;
            return a|0
        }
        m=(g&-8)+j|0;
        if(m>>>0<b>>>0)
        {
            a=0;
            return a|0
        }
        n=m-b|0;
        f=g>>>3;
        do if(g>>>0>=256)
        {
            h=c[a+(j+24)>>2]|0;
            g=c[a+(j+12)>>2]|0;
            do if((g|0)==(l|0))
            {
                e=a+(j+20)|0;
                d=c[e>>2]|0;
                if(!d)
                {
                    e=a+(j+16)|0;
                    d=c[e>>2]|0;
                    if(!d)
                    {
                        k=0;
                        break
                    }
                }
                while(1)
                {
                    f=d+20|0;
                    g=c[f>>2]|0;
                    if(g)
                    {
                        d=g;
                        e=f;
                        continue
                    }
                    g=d+16|0;
                    f=c[g>>2]|0;
                    if(!f)break;
                    else
                    {
                        d=f;
                        e=g
                    }
                }
                if(e>>>0<i>>>0)xb();
                else
                {
                    c[e>>2]=0;
                    k=d;
                    break
                }
            }
            else
            {
                f=c[a+(j+8)>>2]|0;
                if(f>>>0<i>>>0)xb();
                d=f+12|0;
                if((c[d>>2]|0)!=(l|0))xb();
                e=g+8|0;
                if((c[e>>2]|0)==(l|0))
                {
                    c[d>>2]=g;
                    c[e>>2]=f;
                    k=g;
                    break
                }
                else xb()
            }
            while(0);
            if(h)
            {
                d=c[a+(j+28)>>2]|0;
                e=2792+(d<<2)|0;
                if((l|0)==(c[e>>2]|0))
                {
                    c[e>>2]=k;
                    if(!k)
                    {
                        c[623]=c[623]&~(1<<d);
                        break
                    }
                }
                else
                {
                    if(h>>>0<(c[626]|0)>>>0)xb();
                    d=h+16|0;
                    if((c[d>>2]|0)==(l|0))c[d>>2]=k;
                    else c[h+20>>2]=k;
                    if(!k)break
                }
                e=c[626]|0;
                if(k>>>0<e>>>0)xb();
                c[k+24>>2]=h;
                d=c[a+(j+16)>>2]|0;
                do if(d)if(d>>>0<e>>>0)xb();
                else
                {
                    c[k+16>>2]=d;
                    c[d+24>>2]=k;
                    break
                }
                while(0);
                d=c[a+(j+20)>>2]|0;
                if(d)if(d>>>0<(c[626]|0)>>>0)xb();
                else
                {
                    c[k+20>>2]=d;
                    c[d+24>>2]=k;
                    break
                }
            }
        }
        else
        {
            g=c[a+(j+8)>>2]|0;
            e=c[a+(j+12)>>2]|0;
            d=2528+(f<<1<<2)|0;
            if((g|0)!=(d|0))
            {
                if(g>>>0<i>>>0)xb();
                if((c[g+12>>2]|0)!=(l|0))xb()
            }
            if((e|0)==(g|0))
            {
                c[622]=c[622]&~(1<<f);
                break
            }
            if((e|0)!=(d|0))
            {
                if(e>>>0<i>>>0)xb();
                d=e+8|0;
                if((c[d>>2]|0)==(l|0))h=d;
                else xb()
            }
            else h=e+8|0;
            c[g+12>>2]=e;
            c[h>>2]=g
        }
        while(0);
        if(n>>>0<16)
        {
            c[o>>2]=m|p&1|2;
            b=a+(m|4)|0;
            c[b>>2]=c[b>>2]|1;
            return a|0
        }
        else
        {
            c[o>>2]=p&1|b|2;
            c[a+(b+4)>>2]=n|3;
            p=a+(m|4)|0;
            c[p>>2]=c[p>>2]|1;
            de(a+b|0,n);
            return a|0
        }
        return 0
    }
    function de(a,b)
    {
        a=a|0;
        b=b|0;
        var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0;
        q=a+b|0;
        d=c[a+4>>2]|0;
        do if(!(d&1))
        {
            k=c[a>>2]|0;
            if(!(d&3))return;
            n=a+(0-k)|0;
            m=k+b|0;
            j=c[626]|0;
            if(n>>>0<j>>>0)xb();
            if((n|0)==(c[627]|0))
            {
                g=a+(b+4)|0;
                d=c[g>>2]|0;
                if((d&3|0)!=3)
                {
                    t=n;
                    h=m;
                    break
                }
                c[624]=m;
                c[g>>2]=d&-2;
                c[a+(4-k)>>2]=m|1;
                c[q>>2]=m;
                return
            }
            f=k>>>3;
            if(k>>>0<256)
            {
                e=c[a+(8-k)>>2]|0;
                g=c[a+(12-k)>>2]|0;
                d=2528+(f<<1<<2)|0;
                if((e|0)!=(d|0))
                {
                    if(e>>>0<j>>>0)xb();
                    if((c[e+12>>2]|0)!=(n|0))xb()
                }
                if((g|0)==(e|0))
                {
                    c[622]=c[622]&~(1<<f);
                    t=n;
                    h=m;
                    break
                }
                if((g|0)!=(d|0))
                {
                    if(g>>>0<j>>>0)xb();
                    d=g+8|0;
                    if((c[d>>2]|0)==(n|0))i=d;
                    else xb()
                }
                else i=g+8|0;
                c[e+12>>2]=g;
                c[i>>2]=e;
                t=n;
                h=m;
                break
            }
            i=c[a+(24-k)>>2]|0;
            e=c[a+(12-k)>>2]|0;
            do if((e|0)==(n|0))
            {
                e=16-k|0;
                g=a+(e+4)|0;
                d=c[g>>2]|0;
                if(!d)
                {
                    g=a+e|0;
                    d=c[g>>2]|0;
                    if(!d)
                    {
                        l=0;
                        break
                    }
                }
                while(1)
                {
                    e=d+20|0;
                    f=c[e>>2]|0;
                    if(f)
                    {
                        d=f;
                        g=e;
                        continue
                    }
                    e=d+16|0;
                    f=c[e>>2]|0;
                    if(!f)break;
                    else
                    {
                        d=f;
                        g=e
                    }
                }
                if(g>>>0<j>>>0)xb();
                else
                {
                    c[g>>2]=0;
                    l=d;
                    break
                }
            }
            else
            {
                f=c[a+(8-k)>>2]|0;
                if(f>>>0<j>>>0)xb();
                d=f+12|0;
                if((c[d>>2]|0)!=(n|0))xb();
                g=e+8|0;
                if((c[g>>2]|0)==(n|0))
                {
                    c[d>>2]=e;
                    c[g>>2]=f;
                    l=e;
                    break
                }
                else xb()
            }
            while(0);
            if(i)
            {
                d=c[a+(28-k)>>2]|0;
                g=2792+(d<<2)|0;
                if((n|0)==(c[g>>2]|0))
                {
                    c[g>>2]=l;
                    if(!l)
                    {
                        c[623]=c[623]&~(1<<d);
                        t=n;
                        h=m;
                        break
                    }
                }
                else
                {
                    if(i>>>0<(c[626]|0)>>>0)xb();
                    d=i+16|0;
                    if((c[d>>2]|0)==(n|0))c[d>>2]=l;
                    else c[i+20>>2]=l;
                    if(!l)
                    {
                        t=n;
                        h=m;
                        break
                    }
                }
                e=c[626]|0;
                if(l>>>0<e>>>0)xb();
                c[l+24>>2]=i;
                d=16-k|0;
                g=c[a+d>>2]|0;
                do if(g)if(g>>>0<e>>>0)xb();
                else
                {
                    c[l+16>>2]=g;
                    c[g+24>>2]=l;
                    break
                }
                while(0);
                d=c[a+(d+4)>>2]|0;
                if(d)if(d>>>0<(c[626]|0)>>>0)xb();
                else
                {
                    c[l+20>>2]=d;
                    c[d+24>>2]=l;
                    t=n;
                    h=m;
                    break
                }
                else
                {
                    t=n;
                    h=m
                }
            }
            else
            {
                t=n;
                h=m
            }
        }
        else
        {
            t=a;
            h=b
        }
        while(0);
        j=c[626]|0;
        if(q>>>0<j>>>0)xb();
        d=a+(b+4)|0;
        g=c[d>>2]|0;
        if(!(g&2))
        {
            if((q|0)==(c[628]|0))
            {
                s=(c[625]|0)+h|0;
                c[625]=s;
                c[628]=t;
                c[t+4>>2]=s|1;
                if((t|0)!=(c[627]|0))return;
                c[627]=0;
                c[624]=0;
                return
            }
            if((q|0)==(c[627]|0))
            {
                s=(c[624]|0)+h|0;
                c[624]=s;
                c[627]=t;
                c[t+4>>2]=s|1;
                c[t+s>>2]=s;
                return
            }
            h=(g&-8)+h|0;
            f=g>>>3;
            do if(g>>>0>=256)
            {
                i=c[a+(b+24)>>2]|0;
                e=c[a+(b+12)>>2]|0;
                do if((e|0)==(q|0))
                {
                    g=a+(b+20)|0;
                    d=c[g>>2]|0;
                    if(!d)
                    {
                        g=a+(b+16)|0;
                        d=c[g>>2]|0;
                        if(!d)
                        {
                            p=0;
                            break
                        }
                    }
                    while(1)
                    {
                        e=d+20|0;
                        f=c[e>>2]|0;
                        if(f)
                        {
                            d=f;
                            g=e;
                            continue
                        }
                        e=d+16|0;
                        f=c[e>>2]|0;
                        if(!f)break;
                        else
                        {
                            d=f;
                            g=e
                        }
                    }
                    if(g>>>0<j>>>0)xb();
                    else
                    {
                        c[g>>2]=0;
                        p=d;
                        break
                    }
                }
                else
                {
                    f=c[a+(b+8)>>2]|0;
                    if(f>>>0<j>>>0)xb();
                    d=f+12|0;
                    if((c[d>>2]|0)!=(q|0))xb();
                    g=e+8|0;
                    if((c[g>>2]|0)==(q|0))
                    {
                        c[d>>2]=e;
                        c[g>>2]=f;
                        p=e;
                        break
                    }
                    else xb()
                }
                while(0);
                if(i)
                {
                    d=c[a+(b+28)>>2]|0;
                    g=2792+(d<<2)|0;
                    if((q|0)==(c[g>>2]|0))
                    {
                        c[g>>2]=p;
                        if(!p)
                        {
                            c[623]=c[623]&~(1<<d);
                            break
                        }
                    }
                    else
                    {
                        if(i>>>0<(c[626]|0)>>>0)xb();
                        d=i+16|0;
                        if((c[d>>2]|0)==(q|0))c[d>>2]=p;
                        else c[i+20>>2]=p;
                        if(!p)break
                    }
                    g=c[626]|0;
                    if(p>>>0<g>>>0)xb();
                    c[p+24>>2]=i;
                    d=c[a+(b+16)>>2]|0;
                    do if(d)if(d>>>0<g>>>0)xb();
                    else
                    {
                        c[p+16>>2]=d;
                        c[d+24>>2]=p;
                        break
                    }
                    while(0);
                    d=c[a+(b+20)>>2]|0;
                    if(d)if(d>>>0<(c[626]|0)>>>0)xb();
                    else
                    {
                        c[p+20>>2]=d;
                        c[d+24>>2]=p;
                        break
                    }
                }
            }
            else
            {
                e=c[a+(b+8)>>2]|0;
                g=c[a+(b+12)>>2]|0;
                d=2528+(f<<1<<2)|0;
                if((e|0)!=(d|0))
                {
                    if(e>>>0<j>>>0)xb();
                    if((c[e+12>>2]|0)!=(q|0))xb()
                }
                if((g|0)==(e|0))
                {
                    c[622]=c[622]&~(1<<f);
                    break
                }
                if((g|0)!=(d|0))
                {
                    if(g>>>0<j>>>0)xb();
                    d=g+8|0;
                    if((c[d>>2]|0)==(q|0))o=d;
                    else xb()
                }
                else o=g+8|0;
                c[e+12>>2]=g;
                c[o>>2]=e
            }
            while(0);
            c[t+4>>2]=h|1;
            c[t+h>>2]=h;
            if((t|0)==(c[627]|0))
            {
                c[624]=h;
                return
            }
        }
        else
        {
            c[d>>2]=g&-2;
            c[t+4>>2]=h|1;
            c[t+h>>2]=h
        }
        d=h>>>3;
        if(h>>>0<256)
        {
            g=d<<1;
            f=2528+(g<<2)|0;
            e=c[622]|0;
            d=1<<d;
            if(e&d)
            {
                d=2528+(g+2<<2)|0;
                e=c[d>>2]|0;
                if(e>>>0<(c[626]|0)>>>0)xb();
                else
                {
                    r=d;
                    s=e
                }
            }
            else
            {
                c[622]=e|d;
                r=2528+(g+2<<2)|0;
                s=f
            }
            c[r>>2]=t;
            c[s+12>>2]=t;
            c[t+8>>2]=s;
            c[t+12>>2]=f;
            return
        }
        d=h>>>8;
        if(d)if(h>>>0>16777215)g=31;
        else
        {
            r=(d+1048320|0)>>>16&8;
            s=d<<r;
            q=(s+520192|0)>>>16&4;
            s=s<<q;
            g=(s+245760|0)>>>16&2;
            g=14-(q|r|g)+(s<<g>>>15)|0;
            g=h>>>(g+7|0)&1|g<<1
        }
        else g=0;
        d=2792+(g<<2)|0;
        c[t+28>>2]=g;
        c[t+20>>2]=0;
        c[t+16>>2]=0;
        e=c[623]|0;
        f=1<<g;
        if(!(e&f))
        {
            c[623]=e|f;
            c[d>>2]=t;
            c[t+24>>2]=d;
            c[t+12>>2]=t;
            c[t+8>>2]=t;
            return
        }
        d=c[d>>2]|0;
        a:do if((c[d+4>>2]&-8|0)!=(h|0))
        {
            g=h<<((g|0)==31?0:25-(g>>>1)|0);
            while(1)
            {
                e=d+16+(g>>>31<<2)|0;
                f=c[e>>2]|0;
                if(!f)break;
                if((c[f+4>>2]&-8|0)==(h|0))
                {
                    d=f;
                    break a
                }
                else
                {
                    g=g<<1;
                    d=f
                }
            }
            if(e>>>0<(c[626]|0)>>>0)xb();
            c[e>>2]=t;
            c[t+24>>2]=d;
            c[t+12>>2]=t;
            c[t+8>>2]=t;
            return
        }
        while(0);
        e=d+8|0;
        f=c[e>>2]|0;
        s=c[626]|0;
        if(!(f>>>0>=s>>>0&d>>>0>=s>>>0))xb();
        c[f+12>>2]=t;
        c[e>>2]=t;
        c[t+8>>2]=f;
        c[t+12>>2]=d;
        c[t+24>>2]=0;
        return
    }
    function ee(a)
    {
        a=a|0;
        var b=0,d=0,e=0,f=0,g=0,h=0;
        b=c[p>>2]|0;
        ge(3696,b,3752);
        c[746]=4660;
        c[748]=4680;
        c[747]=0;
        e=c[1162]|0;
        gf(2984+e|0,3696);
        c[2984+(e+72)>>2]=0;
        c[2984+(e+76)>>2]=-1;
        e=c[q>>2]|0;
        he(3800,e,3760);
        c[768]=4740;
        c[769]=4760;
        h=c[1182]|0;
        gf(3072+h|0,3800);
        f=h+72|0;
        c[3072+f>>2]=0;
        a=h+76|0;
        c[3072+a>>2]=-1;
        d=c[o>>2]|0;
        he(3848,d,3768);
        c[790]=4740;
        c[791]=4760;
        gf(3160+h|0,3848);
        c[3160+f>>2]=0;
        c[3160+a>>2]=-1;
        g=c[3160+((c[(c[790]|0)+-12>>2]|0)+24)>>2]|0;
        c[812]=4740;
        c[813]=4760;
        gf(3248+h|0,g);
        c[3248+f>>2]=0;
        c[3248+a>>2]=-1;
        c[2984+((c[(c[746]|0)+-12>>2]|0)+72)>>2]=3072;
        a=3160+((c[(c[790]|0)+-12>>2]|0)+4)|0;
        c[a>>2]=c[a>>2]|8192;
        c[3160+((c[(c[790]|0)+-12>>2]|0)+72)>>2]=3072;
        ie(3896,b,3776);
        c[834]=4700;
        c[836]=4720;
        c[835]=0;
        b=c[1172]|0;
        gf(3336+b|0,3896);
        c[3336+(b+72)>>2]=0;
        c[3336+(b+76)>>2]=-1;
        je(3952,e,3784);
        c[856]=4780;
        c[857]=4800;
        e=c[1192]|0;
        gf(3424+e|0,3952);
        b=e+72|0;
        c[3424+b>>2]=0;
        a=e+76|0;
        c[3424+a>>2]=-1;
        je(4e3,d,3792);
        c[878]=4780;
        c[879]=4800;
        gf(3512+e|0,4e3);
        c[3512+b>>2]=0;
        c[3512+a>>2]=-1;
        d=c[3512+((c[(c[878]|0)+-12>>2]|0)+24)>>2]|0;
        c[900]=4780;
        c[901]=4800;
        gf(3600+e|0,d);
        c[3600+b>>2]=0;
        c[3600+a>>2]=-1;
        c[3336+((c[(c[834]|0)+-12>>2]|0)+72)>>2]=3424;
        a=3512+((c[(c[878]|0)+-12>>2]|0)+4)|0;
        c[a>>2]=c[a>>2]|8192;
        c[3512+((c[(c[878]|0)+-12>>2]|0)+72)>>2]=3424;
        return
    }
    function fe(a)
    {
        a=a|0;
        Qf(3072)|0;
        Qf(3248)|0;
        Vf(3424)|0;
        Vf(3600)|0;
        return
    }
    function ge(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0;
        f=i;
        i=i+16|0;
        h=f+4|0;
        g=f;
        kf(b);
        c[b>>2]=4424;
        c[b+32>>2]=d;
        c[b+40>>2]=e;
        c[b+48>>2]=-1;
        a[b+52>>0]=0;
        qk(h,b+4|0);
        c[g>>2]=c[h>>2];
        Be(b,g);
        rk(g);
        i=f;
        return
    }
    function he(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0;
        f=i;
        i=i+16|0;
        h=f+4|0;
        g=f;
        kf(b);
        c[b>>2]=4312;
        c[b+32>>2]=d;
        qk(h,b+4|0);
        c[g>>2]=c[h>>2];
        d=tk(g,6648)|0;
        rk(g);
        c[b+36>>2]=d;
        c[b+40>>2]=e;
        a[b+44>>0]=(Qb[c[(c[d>>2]|0)+28>>2]&63](d)|0)&1;
        i=f;
        return
    }
    function ie(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0;
        f=i;
        i=i+16|0;
        h=f+4|0;
        g=f;
        zf(b);
        c[b>>2]=4168;
        c[b+32>>2]=d;
        c[b+40>>2]=e;
        c[b+48>>2]=-1;
        a[b+52>>0]=0;
        qk(h,b+4|0);
        c[g>>2]=c[h>>2];
        qe(b,g);
        rk(g);
        i=f;
        return
    }
    function je(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0,g=0,h=0;
        f=i;
        i=i+16|0;
        h=f+4|0;
        g=f;
        zf(b);
        c[b>>2]=4056;
        c[b+32>>2]=d;
        qk(h,b+4|0);
        c[g>>2]=c[h>>2];
        d=tk(g,6656)|0;
        rk(g);
        c[b+36>>2]=d;
        c[b+40>>2]=e;
        a[b+44>>0]=(Qb[c[(c[d>>2]|0)+28>>2]&63](d)|0)&1;
        i=f;
        return
    }
    function ke()
    {
        ee(0);
        rb(95,3688,n|0)|0;
        return
    }
    function le(a)
    {
        a=a|0;
        xf(a);
        Lc(a);
        return
    }
    function me(b,d)
    {
        b=b|0;
        d=d|0;
        Qb[c[(c[b>>2]|0)+24>>2]&63](b)|0;
        d=tk(d,6656)|0;
        c[b+36>>2]=d;
        a[b+44>>0]=(Qb[c[(c[d>>2]|0)+28>>2]&63](d)|0)&1;
        return
    }
    function ne(a)
    {
        a=a|0;
        var b=0,d=0,e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0;
        l=i;
        i=i+16|0;
        j=l+8|0;
        h=l;
        d=a+36|0;
        e=a+40|0;
        f=j+8|0;
        g=j;
        b=a+32|0;
        while(1)
        {
            a=c[d>>2]|0;
            a=Xb[c[(c[a>>2]|0)+20>>2]&31](a,c[e>>2]|0,j,f,h)|0;
            m=(c[h>>2]|0)-g|0;
            if((zb(j|0,1,m|0,c[b>>2]|0)|0)!=(m|0))
            {
                a=-1;
                break
            }
            if((a|0)==2)
            {
                a=-1;
                break
            }
            else if((a|0)!=1)
            {
                k=4;
                break
            }
        }
        if((k|0)==4)a=((Ma(c[b>>2]|0)|0)!=0)<<31>>31;
        i=l;
        return a|0
    }
    function oe(b,d,e)
    {
        b=b|0;
        d=d|0;
        e=e|0;
        var f=0;
        a:do if(!(a[b+44>>0]|0))if((e|0)>0)
        {
            f=d;
            d=0;
            while(1)
            {
                if((Wb[c[(c[b>>2]|0)+52>>2]&15](b,c[f>>2]|0)|0)==-1)break a;
                d=d+1|0;
                if((d|0)<(e|0))f=f+4|0;
                else break
            }
        }
        else d=0;
        else d=zb(d|0,4,e|0,c[b+32>>2]|0)|0;
        while(0);
        return d|0
    }
    function pe(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;
        s=i;
        i=i+32|0;
        p=s+16|0;
        e=s;
        o=s+4|0;
        n=s+8|0;
        q=(d|0)==-1;
        a:do if(!q)
        {
            c[e>>2]=d;
            if(a[b+44>>0]|0)if((zb(e|0,4,1,c[b+32>>2]|0)|0)==1)
            {
                r=11;
                break
            }
            else
            {
                e=-1;
                break
            }
            c[o>>2]=p;
            l=e+4|0;
            m=b+36|0;
            g=b+40|0;
            h=p+8|0;
            j=p;
            k=b+32|0;
            while(1)
            {
                b=c[m>>2]|0;
                b=Tb[c[(c[b>>2]|0)+12>>2]&15](b,c[g>>2]|0,e,l,n,p,h,o)|0;
                if((c[n>>2]|0)==(e|0))
                {
                    e=-1;
                    break a
                }
                if((b|0)==3)break;
                f=(b|0)==1;
                if(b>>>0>=2)
                {
                    e=-1;
                    break a
                }
                b=(c[o>>2]|0)-j|0;
                if((zb(p|0,1,b|0,c[k>>2]|0)|0)!=(b|0))
                {
                    e=-1;
                    break a
                }
                if(f)e=f?c[n>>2]|0:e;
                else
                {
                    r=11;
                    break a
                }
            }
            if((zb(e|0,1,1,c[k>>2]|0)|0)!=1)e=-1;
            else r=11
        }
        else r=11;
        while(0);
        if((r|0)==11)e=q?0:d;
        i=s;
        return e|0
    }
    function qe(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0;
        f=tk(d,6656)|0;
        e=b+36|0;
        c[e>>2]=f;
        d=b+44|0;
        c[d>>2]=Qb[c[(c[f>>2]|0)+24>>2]&63](f)|0;
        e=c[e>>2]|0;
        a[b+53>>0]=(Qb[c[(c[e>>2]|0)+28>>2]&63](e)|0)&1;
        return
    }
    function re(a)
    {
        a=a|0;
        xf(a);
        Lc(a);
        return
    }
    function se(a)
    {
        a=a|0;
        return ve(a,0)|0
    }
    function te(a)
    {
        a=a|0;
        return ve(a,1)|0
    }
    function ue(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0;
        m=i;
        i=i+32|0;
        l=m+16|0;
        k=m;
        f=m+4|0;
        g=m+8|0;
        h=b+52|0;
        e=(a[h>>0]|0)!=0;
        a:do if((d|0)==-1)if(e)d=-1;
        else
        {
            d=c[b+48>>2]|0;
            a[h>>0]=(d|0)!=-1&1
        }
        else
        {
            j=b+48|0;
            b:do if(e)
            {
                c[f>>2]=c[j>>2];
                e=c[b+36>>2]|0;
                e=Tb[c[(c[e>>2]|0)+12>>2]&15](e,c[b+40>>2]|0,f,f+4|0,g,l,l+8|0,k)|0;
                if((e|0)==1|(e|0)==2)
                {
                    d=-1;
                    break a
                }
                else if((e|0)==3)
                {
                    a[l>>0]=c[j>>2];
                    c[k>>2]=l+1
                }
                e=b+32|0;
                while(1)
                {
                    f=c[k>>2]|0;
                    if(f>>>0<=l>>>0)break b;
                    b=f+-1|0;
                    c[k>>2]=b;
                    if((Ha(a[b>>0]|0,c[e>>2]|0)|0)==-1)
                    {
                        d=-1;
                        break a
                    }
                }
            }
            while(0);
            c[j>>2]=d;
            a[h>>0]=1
        }
        while(0);
        i=m;
        return d|0
    }
    function ve(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0;
        s=i;
        i=i+32|0;
        r=s+16|0;
        q=s;
        n=s+4|0;
        o=s+8|0;
        g=b+52|0;
        a:do if(a[g>>0]|0)
        {
            f=b+48|0;
            e=c[f>>2]|0;
            if(d)
            {
                c[f>>2]=-1;
                a[g>>0]=0
            }
        }
        else
        {
            e=c[b+44>>2]|0;
            e=(e|0)>1?e:1;
            p=b+32|0;
            if((e|0)>0)
            {
                g=0;
                do
                {
                    f=lb(c[p>>2]|0)|0;
                    if((f|0)==-1)
                    {
                        e=-1;
                        break a
                    }
                    a[r+g>>0]=f;
                    g=g+1|0
                }
                while((g|0)<(e|0))
            }
            b:do if(!(a[b+53>>0]|0))
            {
                k=b+40|0;
                l=b+36|0;
                m=q+4|0;
                while(1)
                {
                    h=c[k>>2]|0;
                    g=h;
                    f=c[g>>2]|0;
                    g=c[g+4>>2]|0;
                    t=c[l>>2]|0;
                    j=r+e|0;
                    h=Tb[c[(c[t>>2]|0)+16>>2]&15](t,h,r,j,n,q,m,o)|0;
                    if((h|0)==3)break;
                    else if((h|0)==2)
                    {
                        e=-1;
                        break a
                    }
                    else if((h|0)!=1)break b;
                    t=c[k>>2]|0;
                    c[t>>2]=f;
                    c[t+4>>2]=g;
                    if((e|0)==8)
                    {
                        e=-1;
                        break a
                    }
                    f=lb(c[p>>2]|0)|0;
                    if((f|0)==-1)
                    {
                        e=-1;
                        break a
                    }
                    a[j>>0]=f;
                    e=e+1|0
                }
                c[q>>2]=a[r>>0]
            }
            else c[q>>2]=a[r>>0];
            while(0);
            if(d)
            {
                e=c[q>>2]|0;
                c[b+48>>2]=e;
                break
            }
            while(1)
            {
                if((e|0)<=0)break;
                e=e+-1|0;
                if((Ha(a[r+e>>0]|0,c[p>>2]|0)|0)==-1)
                {
                    e=-1;
                    break a
                }
            }
            e=c[q>>2]|0
        }
        while(0);
        i=s;
        return e|0
    }
    function we(a)
    {
        a=a|0;
        hf(a);
        Lc(a);
        return
    }
    function xe(b,d)
    {
        b=b|0;
        d=d|0;
        Qb[c[(c[b>>2]|0)+24>>2]&63](b)|0;
        d=tk(d,6648)|0;
        c[b+36>>2]=d;
        a[b+44>>0]=(Qb[c[(c[d>>2]|0)+28>>2]&63](d)|0)&1;
        return
    }
    function ye(a)
    {
        a=a|0;
        var b=0,d=0,e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0;
        l=i;
        i=i+16|0;
        j=l+8|0;
        h=l;
        d=a+36|0;
        e=a+40|0;
        f=j+8|0;
        g=j;
        b=a+32|0;
        while(1)
        {
            a=c[d>>2]|0;
            a=Xb[c[(c[a>>2]|0)+20>>2]&31](a,c[e>>2]|0,j,f,h)|0;
            m=(c[h>>2]|0)-g|0;
            if((zb(j|0,1,m|0,c[b>>2]|0)|0)!=(m|0))
            {
                a=-1;
                break
            }
            if((a|0)==2)
            {
                a=-1;
                break
            }
            else if((a|0)!=1)
            {
                k=4;
                break
            }
        }
        if((k|0)==4)a=((Ma(c[b>>2]|0)|0)!=0)<<31>>31;
        i=l;
        return a|0
    }
    function ze(b,e,f)
    {
        b=b|0;
        e=e|0;
        f=f|0;
        var g=0;
        a:do if(!(a[b+44>>0]|0))if((f|0)>0)
        {
            g=e;
            e=0;
            while(1)
            {
                if((Wb[c[(c[b>>2]|0)+52>>2]&15](b,d[g>>0]|0)|0)==-1)break a;
                e=e+1|0;
                if((e|0)<(f|0))g=g+1|0;
                else break
            }
        }
        else e=0;
        else e=zb(e|0,1,f|0,c[b+32>>2]|0)|0;
        while(0);
        return e|0
    }
    function Ae(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;
        s=i;
        i=i+32|0;
        p=s+16|0;
        e=s+8|0;
        o=s;
        n=s+4|0;
        q=(d|0)==-1;
        a:do if(!q)
        {
            a[e>>0]=d;
            if(a[b+44>>0]|0)if((zb(e|0,1,1,c[b+32>>2]|0)|0)==1)
            {
                r=11;
                break
            }
            else
            {
                e=-1;
                break
            }
            c[o>>2]=p;
            m=e+1|0;
            g=b+36|0;
            h=b+40|0;
            j=p+8|0;
            k=p;
            l=b+32|0;
            while(1)
            {
                b=c[g>>2]|0;
                b=Tb[c[(c[b>>2]|0)+12>>2]&15](b,c[h>>2]|0,e,m,n,p,j,o)|0;
                if((c[n>>2]|0)==(e|0))
                {
                    e=-1;
                    break a
                }
                if((b|0)==3)break;
                f=(b|0)==1;
                if(b>>>0>=2)
                {
                    e=-1;
                    break a
                }
                b=(c[o>>2]|0)-k|0;
                if((zb(p|0,1,b|0,c[l>>2]|0)|0)!=(b|0))
                {
                    e=-1;
                    break a
                }
                if(f)e=f?c[n>>2]|0:e;
                else
                {
                    r=11;
                    break a
                }
            }
            if((zb(e|0,1,1,c[l>>2]|0)|0)!=1)e=-1;
            else r=11
        }
        else r=11;
        while(0);
        if((r|0)==11)e=q?0:d;
        i=s;
        return e|0
    }
    function Be(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0;
        f=tk(d,6648)|0;
        e=b+36|0;
        c[e>>2]=f;
        d=b+44|0;
        c[d>>2]=Qb[c[(c[f>>2]|0)+24>>2]&63](f)|0;
        e=c[e>>2]|0;
        a[b+53>>0]=(Qb[c[(c[e>>2]|0)+28>>2]&63](e)|0)&1;
        return
    }
    function Ce(a)
    {
        a=a|0;
        hf(a);
        Lc(a);
        return
    }
    function De(a)
    {
        a=a|0;
        return Ge(a,0)|0
    }
    function Ee(a)
    {
        a=a|0;
        return Ge(a,1)|0
    }
    function Fe(b,d)
    {
        b=b|0;
        d=d|0;
        var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0;
        m=i;
        i=i+32|0;
        l=m+16|0;
        k=m;
        f=m+8|0;
        g=m+4|0;
        h=b+52|0;
        e=(a[h>>0]|0)!=0;
        a:do if((d|0)==-1)if(e)d=-1;
        else
        {
            d=c[b+48>>2]|0;
            a[h>>0]=(d|0)!=-1&1
        }
        else
        {
            j=b+48|0;
            b:do if(e)
            {
                a[f>>0]=c[j>>2];
                e=c[b+36>>2]|0;
                e=Tb[c[(c[e>>2]|0)+12>>2]&15](e,c[b+40>>2]|0,f,f+1|0,g,l,l+8|0,k)|0;
                if((e|0)==3)
                {
                    a[l>>0]=c[j>>2];
                    c[k>>2]=l+1
                }
                else if((e|0)==1|(e|0)==2)
                {
                    d=-1;
                    break a
                }
                e=b+32|0;
                while(1)
                {
                    f=c[k>>2]|0;
                    if(f>>>0<=l>>>0)break b;
                    b=f+-1|0;
                    c[k>>2]=b;
                    if((Ha(a[b>>0]|0,c[e>>2]|0)|0)==-1)
                    {
                        d=-1;
                        break a
                    }
                }
            }
            while(0);
            c[j>>2]=d;
            a[h>>0]=1
        }
        while(0);
        i=m;
        return d|0
    }
    // EMSCRIPTEN_END_FUNCS
    var Ib=[Co,Ci,Gi,Aj,Ej,Jj,Lj,Co];
    var Jb=[Do,ad,_d,Bf,Gf,oe,Kf,mf,rf,ze,vf,pg,ug,$j,ek,Ok,Qk,Tk,yk,Dk,Fk,Ik,Gc,Do,Do,Do,Do,Do,Do,Do,Do,Do];
    var Kb=[Eo,md,ld,id];
    var Lb=[Fo,Pj,Vj,Fo];
    var Mb=[Go,Oc,Pc,Wc,Zc,Xc,Yc,_c,$c,xf,le,re,hf,we,Ce,jf,yf,Mf,Of,Nf,Pf,Rf,Tf,Sf,Uf,Wf,Yf,Xf,Zf,cg,eg,dg,fg,df,ig,kg,mg,vk,qg,rg,vg,wg,Kg,Lg,ch,dh,rh,sh,Eh,Fh,bi,ci,zi,Bi,Ei,Fi,Ii,Ji,Ti,Ui,cj,dj,nj,oj,yj,zj,Hj,Ij,Nj,Oj,Tj,Uj,Zj,_j,ck,dk,kk,lk,Lk,Mk,em,bl,Dl,El,Fl,Gl,lg,uk,xk,Vk,jl,rl,zl,Al,vd,fe,Bj,wk,on,vn,wn,xn,yn,zn,An,Ke,Ve,ae,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go,Go];
    var Nb=[Ho,me,qe,xe,Be,lf,Af,Mi,Ni,Oi,Pi,Ri,Si,Xi,Yi,Zi,_i,aj,bj,gj,hj,ij,jj,lj,mj,rj,sj,tj,uj,wj,xj,bk,gk,Ll,Nl,Pl,Ml,Ol,Ql,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho,Ho];
    var Ob=[Io,xg,yg,zg,Ag,Bg,Cg,Dg,Eg,Fg,Gg,Hg,Mg,Ng,Og,Pg,Qg,Rg,Sg,Tg,Ug,Vg,Wg,jh,lh,wh,yh,Hh,Ih,Jh,Lh,Nh,ei,fi,gi,ii,ki,Sj,Yj,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io,Io];
    var Pb=[Jo,mh,ph,zh,Bh,Jo,Jo,Jo];
    var Qb=[Ko,Qc,ne,Ff,Hf,If,Ef,se,te,ye,qf,sf,tf,pf,De,Ee,Gh,Sl,Ul,Wl,am,cm,Yl,_l,di,Tl,Vl,Xl,bm,dm,Zl,$l,Ki,Li,Qi,Vi,Wi,$i,ej,fj,kj,pj,qj,vj,fl,gl,il,Hl,Jl,Il,Kl,Zk,_k,al,nl,ol,ql,vl,wl,yl,Ko,Ko,Ko,Ko];
    var Rb=[Lo];
    var Sb=[Mo,td,ud,Mo];
    var Tb=[No,Ph,mi,cl,dl,Wk,Xk,kl,ll,sl,tl,No,No,No,No,No];
    var Ub=[Oo,Sk,zk,Ak,Bk,Hk,Oo,Oo];
    var Vb=[Po,pd,od,nd,Cf,nf,ak,fk];
    var Wb=[Qo,Jf,pe,ue,Lf,uf,Ae,Fe,wf,Nk,Pk,Rk,Ck,Ek,Gk,Qo];
    var Xb=[Ro,ng,sg,eh,fh,kh,qh,th,uh,xh,Ch,Uk,el,hl,Jk,Yk,$k,ml,pl,ul,xl,Ro,Ro,Ro,Ro,Ro,Ro,Ro,Ro,Ro,Ro,Ro];
    var Yb=[So,cd,dd,fd,Df,of,og,tg];
    return
    {
        ___cxa_can_catch:qd,_free:ae,_main:gc,___cxa_is_pointer_type:rd,_i64Add:Wn,_memmove:ao,_i64Subtract:Vn,_memset:Xn,_malloc:$d,_memcpy:_n,_strlen:Yn,_bitshift64Lshr:Zn,_bitshift64Shl:$n,__GLOBAL__sub_I_iostream_cpp:ke,runPostSets:Un,stackAlloc:Zb,stackSave:_b,stackRestore:$b,establishStackSpace:ac,setThrew:bc,setTempRet0:ec,getTempRet0:fc,dynCall_iiiiiiii:lo,dynCall_iiii:mo,dynCall_viiiii:no,dynCall_iiiiiid:oo,dynCall_vi:po,dynCall_vii:qo,dynCall_iiiiiii:ro,dynCall_iiiiid:so,dynCall_ii:to,dynCall_viii:uo,dynCall_v:vo,dynCall_iiiiiiiii:wo,dynCall_iiiii:xo,dynCall_viiiiii:yo,dynCall_iii:zo,dynCall_iiiiii:Ao,dynCall_viiii:Bo
    }
}
)