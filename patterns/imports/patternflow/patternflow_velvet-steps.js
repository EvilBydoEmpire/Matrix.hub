// ===== Patternflow pattern =====
// Title:    Velvet Steps
// Author:   @mikdnom
// Date:     2026-09-30
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Walk=0..1, Direction=0..1, Zoom=0..1, Frost=0..1
// @matrix 64x128
// ===== Patternflow pattern =====
// Title:    Velvet Steps
// Author:   @mikdnom
// Date:     2026-09-30
// Inspiration : A cat walking on glass, seen from below


// Any button: a gentle extra paw press.

function vcClamp(v,a,b){
  return Math.max(a,Math.min(b,v));
}

function vcSmooth(v){
  v=vcClamp(v,0,1);
  return v*v*(3-2*v);
}

function vcHash(n){
  const a=Math.sin(n*127.1+17.3)*43758.5453;
  return a-Math.floor(a);
}

const VC_FEET=[
  [-1,.28,0],
  [1,.23,.5],
  [-1,.78,.75],
  [1,.74,.25]
];

export function setup(p){
  p.W=0;
  p.walk=0;
  p.time=0;
  p.pressPulse=0;

  p.k=[.30,0,.42,.25];
  p.target=[.30,0,.42,.25];

  p.angle=0;
  p.x=0;
  p.y=0;
  p.wait=0;

  p.feet=new Float32Array(24);
}

export function update(dt,input,p){
  dt=vcClamp(Number.isFinite(dt)?dt:0,0,.1);

  const k=(input&&input.knobValues)||[];
  const a=1-Math.exp(-dt/.32);

  for(let i=0;i<4;i++){
    if(Number.isFinite(k[i])){
      p.target[i]=vcClamp(k[i],0,1);
    }

    if(i!==1){
      p.k[i]+=(p.target[i]-p.k[i])*a;
    }
  }

  const turn=p.target[1]*Math.PI*2;

  const delta=Math.atan2(
    Math.sin(turn-p.angle),
    Math.cos(turn-p.angle)
  );

  p.angle+=delta*a;

  const speed=.4+2.4*p.k[0];

  p.time+=dt;
  p.walk+=dt*speed*1.15;

  p.x+=Math.sin(p.angle)*dt*speed*9;
  p.y-=Math.cos(p.angle)*dt*speed*9;

  p.pressPulse*=Math.exp(-dt*3.5);

  const b=(input&&input.btnPressed)||[];

  if(b[0]||b[1]||b[2]||b[3]){
    p.pressPulse=1;
  }
}

function vcAlloc(p,w,h){
  p.W=w;
  p.H=h;
  p.S=Math.min(w,h)/64;

  p.buf=new Float32Array(w*h*3);
  p.grain=new Float32Array(w*h);

  for(let i=0;i<w*h;i++){
    p.grain[i]=vcHash(i)-.5;
  }
}

function vcPose(p,j){
  const f=VC_FEET[j];
  const S=p.S;
  const u=(p.walk+f[2])%1;

  let lift=0;
  let travel;

  if(u<.65){
    travel=-5+10*u/.65;
  }else{
    const s=(u-.65)/.35;
    lift=Math.sin(Math.PI*s);
    travel=5-10*vcSmooth(s);
  }

  const weight=1-lift;
  const side=f[0];
  const i=j*6;

  p.feet[i]=
    p.W*.5+
    side*p.W*(j<2?.205:.26)+
    side*lift*1.3*S;

  p.feet[i+1]=f[1]*p.H+travel*S;
  p.feet[i+2]=lift;

  p.feet[i+3]=
    1+.65*.24*weight-lift*.19+
    p.pressPulse*.10*weight;

  p.feet[i+4]=side*(j<2?.13:.26);
  p.feet[i+5]=weight;
}

function vcPaw(p,j){
  const i=j*6;
  const cx=p.feet[i];
  const cy=p.feet[i+1];
  const lift=p.feet[i+2];
  const scale=p.S*p.feet[i+3];
  const angle=p.feet[i+4];

  const c=Math.cos(angle);
  const s=Math.sin(angle);
  const contact=p.feet[i+5];

  const opacity=1-lift*(.28+p.k[3]*.32);
  const extent=16*scale;

  const x0=Math.max(0,Math.floor(cx-extent));
  const x1=Math.min(p.W-1,Math.ceil(cx+extent));
  const y0=Math.max(0,Math.floor(cy-extent));
  const y1=Math.min(p.H-1,Math.ceil(cy+extent));

  const tint=.72;
  const R=83+144*tint;
  const G=74+69*tint;
  const B=79+83*tint;

  for(let y=y0;y<=y1;y++){
    for(let x=x0;x<=x1;x++){
      const dx=x+.5-cx;
      const dy=y+.5-cy;

      const u=(dx*c+dy*s)/scale;
      const v=(-dx*s+dy*c)/scale;

      const o=(y*p.W+x)*3;

      // 부드러운 발 외곽의 털
      const theta=Math.atan2(v+1,u);

      const tufts=
        .22*Math.sin(theta*5+j*1.7)+
        .12*Math.sin(theta*8+1.2);

      const palm=
        Math.pow(Math.abs((u+.25)/7.4),2.5)+
        Math.pow(Math.abs((v-1.2)/7.8),2.5);

      let furD=(Math.pow(palm,1/2.5)-1)*7.4;

      for(let toe=0;toe<4;toe++){
        const tx=
          toe===0?-5.0:
          toe===1?-1.9:
          toe===2?1.9:5.0;

        const ty=
          toe===0?-4.7:
          toe===1?-7.0:
          toe===2?-7.0:-4.7;

        const q=Math.hypot(
          (u-tx)/3.0,
          (v-ty)/4.0
        );

        const toeD=(q-1)*3.0;

        const blend=
          Math.max(1.25-Math.abs(furD-toeD),0)/1.25;

        furD=
          Math.min(furD,toeD)-
          blend*blend*1.25*.25;
      }

      furD-=tufts;

      const softness=1.15+lift*1.8;

      const fur=
        (1-vcSmooth(
          (furD+softness)/(softness*2)
        ))*opacity;

      if(fur>0){
        const grain=p.grain[y*p.W+x];

        const strands=Math.sin(
          theta*37+Math.hypot(u,v)*1.1+j
        );

        const grey=
          89+grain*9+lift*24+
          strands*1.5*vcSmooth((furD+2.5)/3.5);

        p.buf[o]+=(grey-p.buf[o])*fur;
        p.buf[o+1]+=(grey+3-p.buf[o+1])*fur;
        p.buf[o+2]+=(grey+6-p.buf[o+2])*fur;
      }

      let best=99;
      let bu=0;
      let bv=0;

      // 발가락 젤리
      for(let toe=0;toe<4;toe++){
        const tx=
          toe===0?-5.0:
          toe===1?-1.9:
          toe===2?1.9:5.0;

        const ty=
          toe===0?-4.7:
          toe===1?-7.0:
          toe===2?-7.0:-4.7;

        const rot=(toe-1.5)*.19;
        const cc=Math.cos(rot);
        const ss=Math.sin(rot);

        const ex=u-tx;
        const ey=v-ty;

        const a=(ex*cc+ey*ss)/2.05;
        const b=(-ex*ss+ey*cc)/2.8;
        const q=a*a+b*b;

        if(q<best){
          best=q;
          bu=a;
          bv=b;
        }
      }

      // 중앙 젤리
      let central=99;

      for(let l=0;l<5;l++){
        const px=
          l<2?(l===0?-1.55:1.55):(l-3)*2.05;

        const py=l<2?1.25:3.8;

        const a=(u-px)/(l<2?2.55:2.05);
        const b=(v-py)/(l<2?2.55:1.95);

        central=Math.min(central,a*a+b*b);
      }

      if(central<best){
        best=central;
        bu=u/4.5;
        bv=(v-2.3)/3.6;
      }

      if(best>=1)continue;

      const edge=
        vcClamp(
          (1-best)*(5.5-lift*3.5)*scale,
          0,1
        )*opacity;

      const dome=Math.sqrt(Math.max(0,1-best));

      const shine=Math.exp(
        -(
          (bu+.32)*(bu+.32)*12+
          (bv+.4)*(bv+.4)*15
        )
      );

      const shade=.66+.31*dome-.07*bv;
      const rim=Math.pow(best,4)*contact*16;
      const grain=p.grain[y*p.W+x]*2;

      const rr=
        R*shade+shine*(22+contact*19)+rim+grain;

      const gg=
        G*shade+shine*(24+contact*22)+rim+grain;

      const bb=
        B*shade+shine*(26+contact*23)+rim+grain;

      p.buf[o]+=(rr-p.buf[o])*edge;
      p.buf[o+1]+=(gg-p.buf[o+1])*edge;
      p.buf[o+2]+=(bb-p.buf[o+2])*edge;
    }
  }
}

function vcSegment(x,y,ax,ay,bx,by,r0,r1){
  const dx=bx-ax;
  const dy=by-ay;

  const t=vcClamp(
    ((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy),
    0,1
  );

  return Math.hypot(
    x-ax-t*dx,
    y-ay-t*dy
  )-(r0+(r1-r0)*t);
}

function vcBody(p){
  const frost=p.k[3];
  const phase=p.walk*6.283185;

  for(let y=0;y<p.H;y++){
    for(let x=0;x<p.W;x++){
      const X=x+.5-64;
      const Y=y+.5-80;

      const bend=
        2.4*Math.sin(Y*.043+phase*.5)+
        1.1*Math.sin(Y*.11);

      const width=
        14.3+
        5.4*Math.exp(-Math.pow((Y+21)/18,2))+
        5.8*Math.exp(-Math.pow((Y-23)/17,2));

      const rough=
        .45*Math.sin(Y*.61+X*.08)+
        .22*Math.sin(Y*1.37+X*.23);

      const qx=Math.abs(X-bend)-width-rough+7;
      const qy=Math.abs(Y)-31;

      let d=
        Math.hypot(Math.max(qx,0),Math.max(qy,0))+
        Math.min(Math.max(qx,qy),0)-7;

      const head=(
        Math.hypot(
          (X+2+Y*.025)/12.5,
          (Y+44)/13
        )-1
      )*12;

      d=Math.min(d,head);

      d=Math.min(
        d,vcSegment(X,Y,-7,-49,-10,-61,4,.3)
      );

      d=Math.min(
        d,vcSegment(X,Y,5,-49,8,-59,4,.3)
      );

      // 풍성한 꼬리
      for(let n=0;n<7;n++){
        const a=n/7;
        const b=(n+1)/7;

        const ax=
          2+Math.sin(a*2.8+phase*.12)*17*a;

        const bx=
          2+Math.sin(b*2.8+phase*.12)*17*b;

        d=Math.min(
          d,
          vcSegment(
            X,Y,
            ax,34+a*34,
            bx,34+b*34,
            7.0-a*5.3,
            7.0-b*5.3
          )
        );
      }

      // 다리
      for(let j=0;j<4;j++){
        const side=VC_FEET[j][0];
        const i=j*6;

        const ax=side*7;
        const ay=j<2?-22:24;

        const ex=p.feet[i]-64;
        const ey=p.feet[i+1]-80;

        const mx=ax*.4+ex*.6+side*2;
        const my=ay*.6+ey*.4+4;

        d=Math.min(
          d,vcSegment(X,Y,ax,ay,mx,my,6.8,4.3)
        );

        d=Math.min(
          d,vcSegment(X,Y,mx,my,ex,ey,4.3,3.8)
        );
      }

      const blur=1.0+frost*4.5;

      const a=
        (1-vcSmooth((d+blur)/(2*blur)))*
        (.98-frost*.25);

      const lateral=vcClamp(
        (X-bend)/width,-1,1
      );

      const volume=Math.sqrt(
        Math.max(0,1-lateral*lateral)
      );

      const belly=Math.exp(
        -Math.pow((X-bend)/11,2)-
        Math.pow((Y-2)/36,2)
      );

      const chest=Math.exp(
        -Math.pow((X+1)/11,2)-
        Math.pow((Y+29)/14,2)
      );

      const chin=Math.exp(
        -Math.pow((X+1)/8,2)-
        Math.pow((Y+45)/6,2)
      );

      const stripes=Math.pow(
        .5+.5*Math.sin(
          Y*.48+Math.abs(X-bend)*.29
        ),
        7
      );

      const sideMask=vcSmooth(
        (Math.abs(lateral)-.35)/.6
      );

      const furWave=
        Math.sin(X*2.8+Math.sin(Y*.22)*2+Y*.4)*
        Math.sin(Y*1.6+X*.35);

      const texture=(
        furWave*6+p.grain[y*p.W+x]*8
      )*(1-frost*.75);

      let coat=
        88+32*volume-12*lateral+
        belly*42+chest*27+chin*45-
        stripes*sideMask*25+texture;

      const muzzle=Math.max(
        Math.exp(
          -Math.pow((X+3.2)/3.6,2)-
          Math.pow((Y+50)/2.8,2)
        ),
        Math.exp(
          -Math.pow((X-2.6)/3.6,2)-
          Math.pow((Y+50)/2.8,2)
        )
      );

      coat+=muzzle*25;

      const nose=(
        1-vcSmooth(
          (Math.hypot(X/2,(Y+53)/1.3)-.6)/.5
        )
      )*(1-frost*.7);

      let r=coat+5;
      let g=coat+3;
      let b=coat;

      r+=(126-r)*nose;
      g+=(91-g)*nose;
      b+=(94-b)*nose;

      const mouth=Math.exp(
        -X*X*2-Math.pow((Y+50.6)/1.5,2)
      )*.35;

      r*=1-mouth;
      g*=1-mouth;
      b*=1-mouth;

      const o=(y*p.W+x)*3;

      p.buf[o]+=(r-p.buf[o])*a;
      p.buf[o+1]+=(g-p.buf[o+1])*a;
      p.buf[o+2]+=(b-p.buf[o+2])*a;
    }
  }
}

export function draw(display,p,time){
  if(p.W!==128||p.H!==160){
    vcAlloc(p,128,160);
  }

  p.S=1;

  for(let y=0;y<p.H;y++){
    for(let x=0;x<p.W;x++){
      const o=(y*p.W+x)*3;

      p.buf[o]=235;
      p.buf[o+1]=236;
      p.buf[o+2]=233;
    }
  }

  for(let j=0;j<4;j++){
    vcPose(p,j);

    const i=j*6;

    p.feet[i]=
      64+VC_FEET[j][0]*(j<2?13:16)+
      p.feet[i+2]*VC_FEET[j][0];

    p.feet[i+1]=
      80+(j<2?-26:27)+
      (p.feet[i+1]-VC_FEET[j][1]*p.H);
  }

  vcBody(p);

  for(let j=0;j<4;j++){
    if(p.feet[j*6+2]>.05){
      vcPaw(p,j);
    }
  }

  for(let j=0;j<4;j++){
    if(p.feet[j*6+2]<=.05){
      vcPaw(p,j);
    }
  }

  const w=display.width;
  const h=display.height;
  const base=Math.min(w,h)/64;

  p.viewW=w/base;
  p.viewH=h/base;

  const z=base*(.48+p.k[2]*1.35);
  const c=Math.cos(p.angle);
  const s=Math.sin(p.angle);

  // 실제 고양이 윤곽의 범위를 계산
  let minX=Infinity;
  let maxX=-Infinity;
  let minY=Infinity;
  let maxY=-Infinity;

  for(let sy=0;sy<160;sy++){
    for(let sx=0;sx<128;sx++){
      const o=(sy*128+sx)*3;

      const ink=
        704-p.buf[o]-p.buf[o+1]-p.buf[o+2];

      if(ink<3)continue;

      const lx=(sx-63.5)*z;
      const ly=(sy-79.5)*z;

      const xx=lx*c-ly*s;
      const yy=lx*s+ly*c;

      minX=Math.min(minX,xx);
      maxX=Math.max(maxX,xx);
      minY=Math.min(minY,yy);
      maxY=Math.max(maxY,yy);
    }
  }

  const cx=p.x*base;
  const cy=p.y*base;

  // 한 마리가 완전히 나간 뒤에만 반대편에서 재진입
  const outside=
    cx+maxX < -w/2-1 ||
    cx+minX >  w/2+1 ||
    cy+maxY < -h/2-1 ||
    cy+minY >  h/2+1;

  if(outside){
    const vx=Math.sin(p.angle);
    const vy=-Math.cos(p.angle);

    const tx=
      Math.abs(vx)<.0001
        ? Infinity
        : (w/2+(vx>0?maxX:-minX))/Math.abs(vx);

    const ty=
      Math.abs(vy)<.0001
        ? Infinity
        : (h/2+(vy>0?maxY:-minY))/Math.abs(vy);

    const distance=Math.max(
      0,Math.min(tx,ty)-1
    );

    p.x=-vx*distance/base;
    p.y=-vy*distance/base;
  }

  // 고양이는 한 번만 그립니다.
  for(let y=0;y<h;y++){
    for(let x=0;x<w;x++){
      let r=235;
      let g=236;
      let b=233;

      const dx=x+.5-w/2-p.x*base;
      const dy=y+.5-h/2-p.y*base;

      const u=(dx*c+dy*s)/z+63.5;
      const v=(-dx*s+dy*c)/z+79.5;

      const ix=Math.floor(u);
      const iy=Math.floor(v);

      const fx=u-ix;
      const fy=v-iy;

      if(ix>=0&&ix<127&&iy>=0&&iy<159){
        const a=(iy*128+ix)*3;
        const b0=a+3;
        const c0=a+384;
        const d=c0+3;

        const A=(1-fx)*(1-fy);
        const B=fx*(1-fy);
        const C=(1-fx)*fy;
        const D=fx*fy;

        r=
          p.buf[a]*A+p.buf[b0]*B+
          p.buf[c0]*C+p.buf[d]*D;

        g=
          p.buf[a+1]*A+p.buf[b0+1]*B+
          p.buf[c0+1]*C+p.buf[d+1]*D;

        b=
          p.buf[a+2]*A+p.buf[b0+2]*B+
          p.buf[c0+2]*C+p.buf[d+2]*D;
      }

      const grain=
        (vcHash(y*w+x)-.5)*(1+p.k[3]*2);

      const light=4*x/w+3*y/h;

      display.setPixel(
        x,y,
        vcClamp(r+grain-light,0,255)|0,
        vcClamp(g+grain-light,0,255)|0,
        vcClamp(b+grain-light,0,255)|0
      );
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
