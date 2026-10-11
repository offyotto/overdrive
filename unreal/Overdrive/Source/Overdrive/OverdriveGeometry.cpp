#include "OverdriveGeometry.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace ODGeometry
{
using V = Overdrive::Vec3;
using M = ESurface;
constexpr double Tau = Overdrive::Pi * 2;

class FBuilder
{
public:
    FMesh Mesh;
    void Vertex(V P, V N, uint32 RGB, M Surface)
    {
        Mesh.Sections[static_cast<int32>(Surface)].Vertices.Add({FVector3f(ToUnreal(P)), FVector3f(DirectionToUnreal(N)), Color(RGB), FVector2f(P.X * .1, P.Z * .1)});
    }
    void Tri(V A, V B, V C, uint32 RGB, M Surface = M::Surface, V N = {})
    {
        if (N.Length() < .1) N = (B - A).Cross(C - A).Normal();
        Vertex(A, N, RGB, Surface); Vertex(B, N, RGB, Surface); Vertex(C, N, RGB, Surface);
    }
    void Quad(V A, V B, V C, V D, uint32 RGB, M Surface = M::Surface, V N = {})
    {
        Tri(A, B, C, RGB, Surface, N); Tri(A, C, D, RGB, Surface, N);
    }
    void Box(V P, V Size, uint32 RGB, M Surface = M::Surface)
    {
        const double X=P.X,Y=P.Y,Z=P.Z,W=Size.X/2,H=Size.Y/2,D=Size.Z/2;
        Quad({X-W,Y+H,Z-D},{X-W,Y+H,Z+D},{X+W,Y+H,Z+D},{X+W,Y+H,Z-D},RGB,Surface,{0,1,0});
        Quad({X-W,Y-H,Z-D},{X+W,Y-H,Z-D},{X+W,Y-H,Z+D},{X-W,Y-H,Z+D},RGB,Surface,{0,-1,0});
        Quad({X-W,Y-H,Z+D},{X+W,Y-H,Z+D},{X+W,Y+H,Z+D},{X-W,Y+H,Z+D},RGB,Surface,{0,0,1});
        Quad({X+W,Y-H,Z-D},{X-W,Y-H,Z-D},{X-W,Y+H,Z-D},{X+W,Y+H,Z-D},RGB,Surface,{0,0,-1});
        Quad({X-W,Y-H,Z-D},{X-W,Y-H,Z+D},{X-W,Y+H,Z+D},{X-W,Y+H,Z-D},RGB,Surface,{-1,0,0});
        Quad({X+W,Y-H,Z+D},{X+W,Y-H,Z-D},{X+W,Y+H,Z-D},{X+W,Y+H,Z+D},RGB,Surface,{1,0,0});
    }
    void BevelBox(V P, V Size, double B, uint32 RGB, M Surface = M::Surface)
    {
        const double W=Size.X/2,H=Size.Y/2,D=Size.Z/2;
        const V Outline[]={{-W+B,0,-D},{W-B,0,-D},{W,0,-D+B},{W,0,D-B},{W-B,0,D},{-W+B,0,D},{-W,0,D-B},{-W,0,-D+B}};
        for(int I=0;I<8;++I)
        {
            const V A=Outline[I],C=Outline[(I+1)%8];
            const V A0=P+V{A.X,-H,A.Z},B0=P+V{C.X,-H,C.Z};
            const V A1=P+V{A.X*.9,H,A.Z*.94},B1=P+V{C.X*.9,H,C.Z*.94};
            Quad(A0,A1,B1,B0,RGB,Surface);
            Tri(P+V{0,H,0},A1,B1,RGB,Surface,{0,1,0});
            Tri(P+V{0,-H,0},B0,A0,RGB,Surface,{0,-1,0});
        }
    }
    void Ellipsoid(V P,V Radius,uint32 RGB,int Segments=20,int Rings=12,M Surface=M::Surface)
    {
        const auto Point=[&](int I,int J,V& Pos,V& Normal)
        {
            const double A=I/static_cast<double>(Segments)*Tau,B=J/static_cast<double>(Rings)*Overdrive::Pi;
            const V N{std::sin(B)*std::cos(A),std::cos(B),std::sin(B)*std::sin(A)};
            Pos=P+V{N.X*Radius.X,N.Y*Radius.Y,N.Z*Radius.Z}; Normal=V{N.X/Radius.X,N.Y/Radius.Y,N.Z/Radius.Z}.Normal();
        };
        for(int J=0;J<Rings;++J)for(int I=0;I<Segments;++I)
        {
            V Pts[4],Ns[4];Point(I,J,Pts[0],Ns[0]);Point(I+1,J,Pts[1],Ns[1]);Point(I+1,J+1,Pts[2],Ns[2]);Point(I,J+1,Pts[3],Ns[3]);
            for(int K:{0,1,2,0,2,3})Vertex(Pts[K],Ns[K],RGB,Surface);
        }
    }
    void Cylinder(V A,V B,double R1,double R2,uint32 RGB,int Segments=12,M Surface=M::Surface)
    {
        const V Axis=(B-A).Normal(),U=Axis.Cross(std::abs(Axis.Y)>.9?V{1,0,0}:V{0,1,0}).Normal(),W=Axis.Cross(U);
        const auto Radial=[&](int I){const double T=I/static_cast<double>(Segments)*Tau;return U*std::cos(T)+W*std::sin(T);};
        for(int I=0;I<Segments;++I)
        {
            const V N1=Radial(I),N2=Radial(I+1),P1=A+N1*R1,P2=A+N2*R1,P3=B+N2*R2,P4=B+N1*R2;
            Vertex(P1,N1,RGB,Surface);Vertex(P2,N2,RGB,Surface);Vertex(P3,N2,RGB,Surface);
            Vertex(P1,N1,RGB,Surface);Vertex(P3,N2,RGB,Surface);Vertex(P4,N1,RGB,Surface);
            Tri(A,P2,P1,RGB,Surface,Axis*-1);Tri(B,P4,P3,RGB,Surface,Axis);
        }
    }
    void Torus(V P,V Axis,double Radius,double Tube,uint32 RGB,int Segments=40,int Sides=8,M Surface=M::Surface)
    {
        const V N=Axis.Normal(),U=N.Cross(std::abs(N.Y)>.9?V{1,0,0}:V{0,1,0}).Normal(),W=N.Cross(U);
        const auto Point=[&](int I,int J,V& Pos,V& Normal)
        {
            const double A=I/static_cast<double>(Segments)*Tau,B=J/static_cast<double>(Sides)*Tau;
            const V Radial=U*std::cos(A)+W*std::sin(A);Normal=Radial*std::cos(B)+N*std::sin(B);Pos=P+Radial*Radius+Normal*Tube;
        };
        for(int I=0;I<Segments;++I)for(int J=0;J<Sides;++J)
        {
            V Pts[4],Ns[4];Point(I,J,Pts[0],Ns[0]);Point(I+1,J,Pts[1],Ns[1]);Point(I+1,J+1,Pts[2],Ns[2]);Point(I,J+1,Pts[3],Ns[3]);
            for(int K:{0,1,2,0,2,3})Vertex(Pts[K],Ns[K],RGB,Surface);
        }
    }
    struct FProfile { double Z,Width,Y,Height; };
    static V Spline(const TArray<V>& Points,double T)
    {
        const double U=FMath::Clamp(T,0.0,1.0)*(Points.Num()-1);
        const int32 I=FMath::Min(static_cast<int32>(U),Points.Num()-2);const double A=U-I;
        const V P0=Points[FMath::Max(0,I-1)],P1=Points[I],P2=Points[I+1],P3=Points[FMath::Min(Points.Num()-1,I+2)];
        return (P1*2+(P2-P0)*A+(P0*2-P1*5+P2*4-P3)*(A*A)+(P1*3-P0-P2*3+P3)*(A*A*A))*.5;
    }
    template<typename F> void Grid(F Point,int UCount,int VCount,uint32 RGB,M Surface)
    {
        for(int J=0;J<VCount;++J)for(int I=0;I<UCount;++I)
        {
            V Pts[4],Ns[4];
            Point(I/static_cast<double>(UCount),J/static_cast<double>(VCount),Pts[0],Ns[0]);
            Point((I+1)/static_cast<double>(UCount),J/static_cast<double>(VCount),Pts[1],Ns[1]);
            Point((I+1)/static_cast<double>(UCount),(J+1)/static_cast<double>(VCount),Pts[2],Ns[2]);
            Point(I/static_cast<double>(UCount),(J+1)/static_cast<double>(VCount),Pts[3],Ns[3]);
            for(int K:{0,1,2,0,2,3})Vertex(Pts[K],Ns[K],RGB,Surface);
        }
    }
    void Loft(V Offset,const TArray<FProfile>& Profiles,uint32 RGB,M Surface=M::Paint,int Steps=32,int Sides=40)
    {
        TArray<V> Shape;for(const auto& P:Profiles)Shape.Add({P.Width,P.Y,P.Height});
        const auto At=[&](double T,double A)
        {
            const V S=Spline(Shape,T);const double Z=FMath::Lerp(Profiles[0].Z,Profiles.Last().Z,FMath::Clamp(T,0.0,1.0));
            const auto Curve=[](double X){return std::copysign(std::pow(std::abs(X),.68),X);};
            return Offset+V{Curve(std::sin(A))*S.X,S.Y+Curve(std::cos(A))*S.Z,Z};
        };
        Grid([&](double T,double A,V& Pos,V& N)
        {
            const double Angle=A*Tau;Pos=At(T,Angle);
            N=(At(T+.0001,Angle)-At(T-.0001,Angle)).Cross(At(T,Angle+.0001)-At(T,Angle-.0001)).Normal();
        },Steps,Sides,RGB,Surface);
        for(int End:{0,1})for(int I=0;I<Sides;++I)
        {
            const V Center=Offset+V{0,Shape[End?Shape.Num()-1:0].Y,Profiles[End?Profiles.Num()-1:0].Z};
            Tri(Center,At(End,I*Tau/Sides),At(End,(I+1)*Tau/Sides),RGB,Surface,{0,0,End?1.0:-1.0});
        }
    }
    void Tube(const TArray<V>& Points,double Radius,uint32 RGB,M Surface=M::Metal,int Steps=24,int Sides=10)
    {
        const auto At=[&](double T,double Angle,V& Pos,V& N)
        {
            const V Axis=(Spline(Points,T+.0001)-Spline(Points,T-.0001)).Normal();
            const V U=Axis.Cross(std::abs(Axis.Y)>.92?V{0,0,1}:V{0,1,0}).Normal(),W=Axis.Cross(U);
            N=U*std::cos(Angle)+W*std::sin(Angle);Pos=Spline(Points,T)+N*Radius;
        };
        Grid([&](double T,double A,V& Pos,V& N){At(T,A*Tau,Pos,N);},Steps,Sides,RGB,Surface);
        for(int End:{0,1})for(int I=0;I<Sides;++I)
        {
            V A,B,N;At(End,I*Tau/Sides,A,N);At(End,(I+1)*Tau/Sides,B,N);
            const V Axis=(Spline(Points,End+.0001)-Spline(Points,End-.0001)).Normal();
            Tri(Points[End?Points.Num()-1:0],A,B,RGB,Surface,Axis*(End?1:-1));
        }
    }
    void RoundBox(V P,V Size,double Radius,uint32 RGB,M Surface=M::Surface,int Steps=8)
    {
        const V H=Size*.5;Radius=FMath::Min(Radius,FMath::Min3(H.X,H.Y,H.Z)*.95);
        const auto Edge=[&](double T,double Half)
        {
            if(T<.3)return -Half+Radius*T/.3;
            if(T>.7)return Half-Radius+Radius*(T-.7)/.3;
            return FMath::Lerp(-Half+Radius,Half-Radius,(T-.3)/.4);
        };
        for(int Axis=0;Axis<3;++Axis)for(int Side:{-1,1})
        {
            Grid([&](double U,double W,V& Pos,V& N)
            {
                V Q=Axis==0?V{Side*H.X,Edge(U,H.Y),Edge(W,H.Z)}:Axis==1?V{Edge(U,H.X),Side*H.Y,Edge(W,H.Z)}:V{Edge(U,H.X),Edge(W,H.Y),Side*H.Z};
                const V C{FMath::Clamp(Q.X,-H.X+Radius,H.X-Radius),FMath::Clamp(Q.Y,-H.Y+Radius,H.Y-Radius),FMath::Clamp(Q.Z,-H.Z+Radius,H.Z-Radius)};
                N=(Q-C).Normal();Pos=P+C+N*Radius;
            },Steps,Steps,RGB,Surface);
        }
    }
    void ShellPatch(V P,V Radius,double AzimuthMin,double AzimuthMax,double PolarMin,double PolarMax,uint32 RGB,M Surface,int Segments=40,int Rings=16)
    {
        Grid([&](double U,double W,V& Pos,V& N)
        {
            const double A=FMath::Lerp(AzimuthMin,AzimuthMax,U),B=FMath::Lerp(PolarMin,PolarMax,W);
            const V Unit{std::sin(B)*std::sin(A),std::cos(B),std::sin(B)*std::cos(A)};
            Pos=P+V{Unit.X*Radius.X,Unit.Y*Radius.Y,Unit.Z*Radius.Z};N=V{Unit.X/Radius.X,Unit.Y/Radius.Y,Unit.Z/Radius.Z}.Normal();
        },Segments,Rings,RGB,Surface);
    }
    void Lathe(const TArray<FVector2D>& Profile,uint32 RGB,M Surface,int Segments=64)
    {
        for(int J=0;J<Profile.Num();++J)for(int I=0;I<Segments;++I)
        {
            const auto At=[&](int A,int B,V& Pos,V& N)
            {
                const int K=B%Profile.Num();const FVector2D P=Profile[K],D=Profile[(K+1)%Profile.Num()]-Profile[(K+Profile.Num()-1)%Profile.Num()];
                const double Angle=A*Tau/Segments;Pos={P.X,P.Y*std::cos(Angle),P.Y*std::sin(Angle)};
                N=V{-D.Y,D.X*std::cos(Angle),D.X*std::sin(Angle)}.Normal();
            };
            V Pts[4],Ns[4];At(I,J,Pts[0],Ns[0]);At(I+1,J,Pts[1],Ns[1]);At(I+1,J+1,Pts[2],Ns[2]);At(I,J+1,Pts[3],Ns[3]);
            for(int K:{0,1,2,0,2,3})Vertex(Pts[K],Ns[K],RGB,Surface);
        }
    }
    void Diamond(V P,double Size,uint32 RGB,M Surface=M::Glow)
    {
        for(int I=0;I<4;++I)
        {
            const V A=P+V{std::cos(I*Tau/4)*Size,0,std::sin(I*Tau/4)*Size};
            const V B=P+V{std::cos((I+1)*Tau/4)*Size,0,std::sin((I+1)*Tau/4)*Size};
            Tri(P+V{0,Size*1.35,0},B,A,RGB,Surface);Tri(P+V{0,-Size*1.35,0},A,B,RGB,Surface);
        }
    }
};

const TCHAR* MaterialName(ESurface Surface)
{
    switch(Surface)
    {
        case M::Paint:return TEXT("MI_Paint");case M::Metal:return TEXT("MI_Metal");
        case M::Rubber:return TEXT("MI_Rubber");case M::Visor:return TEXT("MI_Visor");
        case M::Glow:return TEXT("MI_Glow");default:return TEXT("MI_Surface");
    }
}

void PrepareMeshForUnreal(FMesh& Mesh)
{
    for(FSection& Section:Mesh.Sections)
    {
        TArray<FVertex> Prepared;Prepared.Reserve(Section.Vertices.Num());
        for(int32 I=0;I+2<Section.Vertices.Num();I+=3)
        {
            FVertex A=Section.Vertices[I],B=Section.Vertices[I+1],C=Section.Vertices[I+2];
            FVector3f Face=FVector3f::CrossProduct(C.Position-A.Position,B.Position-A.Position);
            if(Face.SizeSquared()<1.e-8f)continue;
            const FVector3f Normal=(A.Normal+B.Normal+C.Normal).GetSafeNormal();
            if(FVector3f::DotProduct(Face,Normal)<0)Swap(B,C);
            const FVector3f Tangent=(B.Position-A.Position).GetSafeNormal();
            const FVector3f Bitangent=FVector3f::CrossProduct(Normal,Tangent).GetSafeNormal();
            for(FVertex* Vertex:{&A,&B,&C})
            {
                const FVector3f Local=Vertex->Position-A.Position;
                Vertex->UV=FVector2f(FVector3f::DotProduct(Local,Tangent)*.01f,FVector3f::DotProduct(Local,Bitangent)*.01f);
            }
            Prepared.Add(A);Prepared.Add(B);Prepared.Add(C);
        }
        Section.Vertices=MoveTemp(Prepared);
    }
}

void BuildKartAssets(TMap<FName,FMesh>& Output)
{
    constexpr uint32 Carbon=0x18242d,Graphite=0x303b43,Ivory=0xf2efe4,Silver=0xbbcbd1,Titanium=0x758d98,Gold=0xd4af73,Cyan=0x87e9ec;
    FBuilder Kart,Wheel,Steering;
    Kart.RoundBox({0,.40,0},{2.10,.20,3.95},.09,Carbon,M::Surface);
    Kart.Loft({0,0,0},{{-1.76,.64,.65,.14},{-.85,.80,.70,.22},{0,.85,.72,.24},{.85,.72,.71,.18},{1.87,.56,.62,.12}},Graphite,M::Surface,32,32);
    Kart.Loft({},{{.20,.63,.89,.21},{.62,.68,1.00,.23},{1.04,.57,.95,.19},{1.46,.48,.83,.14},{1.88,.55,.70,.10}},0xffffff,M::Paint,40,48);
    Kart.RoundBox({0,.47,1.94},{2.33,.13,.45},.06,Carbon,M::Surface);
    Kart.Tube({{-1.12,.62,1.69},{-1.13,.63,1.96},{-.66,.63,2.14},{0,.63,2.18},{.66,.63,2.14},{1.13,.63,1.96},{1.12,.62,1.69}},.045,Titanium,M::Metal,44,10);
    const auto Stripe=[&](const TArray<V>& Path,double Width,uint32 RGB)
    {
        Kart.Grid([&](double U,double W,V& Pos,V& N)
        {
            const V T=FBuilder::Spline(Path,U+.0001)-FBuilder::Spline(Path,U-.0001);
            Pos=FBuilder::Spline(Path,U)+V{(W-.5)*Width,0,0};N=T.Cross({1,0,0}).Normal();
        },28,1,RGB,M::Surface);
    };
    for(int Side:{-1,1})
    {
        Stripe({{Side*.145,1.115,.20},{Side*.145,1.245,.62},{Side*.145,1.155,1.04},{Side*.145,.985,1.46},{Side*.145,.815,1.88}},.13,Ivory);
        Kart.Loft({Side*.94,0,0},{{-.62,.12,.67,.13},{-.31,.25,.73,.22},{0,.29,.73,.21},{.31,.22,.70,.16},{.62,.10,.66,.10}},0xffffff,M::Paint,28,36);
        Kart.Tube({{Side*1.00,.525,-.56},{Side*1.21,.515,-.26},{Side*1.23,.51,.22},{Side*1.05,.51,.60}},.026,Ivory,M::Surface,20,8);
        Kart.RoundBox({Side*1.203,.76,-.12},{.027,.19,.47},.012,Carbon,M::Rubber,5);
        for(int K=0;K<5;++K)Kart.Cylinder({Side*1.225,.67,-.31+K*.09},{Side*1.225,.82,-.35+K*.09},.012,.012,Titanium,8,M::Metal);
        Kart.RoundBox({Side*.75,.83,1.57},{.37,.18,.15},.065,Graphite,M::Surface);
        Kart.RoundBox({Side*.75,.855,1.654},{.29,.046,.022},.010,Ivory,M::Glow,5);
        Kart.RoundBox({Side*.75,.794,1.654},{.23,.021,.022},.009,Cyan,M::Glow,4);
        Kart.RoundBox({Side*.99,.76,1.86},{.095,.31,.40},.04,0xffffff,M::Paint,5);
        Kart.Cylinder({Side*.88,.50,-1.22},{Side*.88,.50,1.23},.037,.037,Titanium,12,M::Metal);
        for(double Z:{-1.23,1.23})
        {
            Kart.Cylinder({Side*.69,.49,Z-.30},{Side*1.23,.56,Z},.036,.036,Silver,12,M::Metal);
            Kart.Cylinder({Side*.69,.49,Z+.27},{Side*1.23,.56,Z},.036,.036,Silver,12,M::Metal);
            Kart.Cylinder({Side*.66,.77,Z-.15},{Side*1.20,.68,Z},.025,.025,Carbon,10,M::Metal);
            const V A{Side*.83,.91,Z-.08},B{Side*1.16,.53,Z};
            Kart.Cylinder(A,B,.063,.047,Graphite,12,M::Metal);
            Kart.Cylinder(A,A+(B-A)*.35,.085,.085,Gold,12,M::Metal);
            const V Axis=(B-A).Normal(),U=Axis.Cross({0,0,1}).Normal(),W=Axis.Cross(U);
            TArray<V> Coil;
            for(int K=0;K<=48;++K){const double T=K/48.0,Around=T*Tau*5;Coil.Add(A+(B-A)*(.20+T*.67)+(U*std::cos(Around)+W*std::sin(Around))*.083);}
            Kart.Tube(Coil,.014,Cyan,M::Metal,48,6);
        }
    }
    Kart.Cylinder({-1.30,.63,-1.23},{1.30,.63,-1.23},.055,.055,Titanium,16,M::Metal);
    Kart.RoundBox({0,.87,-1.27},{1.22,.53,.79},.12,Graphite,M::Metal);
    for(int I=0;I<8;++I)Kart.RoundBox({0,1.125,-1.55+I*.081},{1.08,.055,.034},.014,Titanium,M::Metal,4);
    Kart.RoundBox({0,1.185,-1.29},{.40,.056,.32},.025,0xffffff,M::Paint,6);
    for(int Side:{-1,1})
    {
        Kart.Tube({{Side*.53,.89,-1.22},{Side*.80,.70,-1.47},{Side*.78,.76,-1.78},{Side*.65,.78,-2.08}},.103,Titanium,M::Metal,28,16);
        Kart.Torus({Side*.65,.78,-2.08},{0,0,1},.108,.024,Silver,32,8,M::Metal);
        Kart.Cylinder({Side*.65,.78,-2.087},{Side*.65,.78,-2.102},.087,.087,Carbon,24,M::Rubber);
        Kart.Torus({Side*.65,.78,-2.104},{0,0,1},.065,.014,Cyan,24,8,M::Glow);
        Kart.RoundBox({Side*.70,1.26,-1.57},{.085,.65,.19},.035,Carbon,M::Metal,5);
        Kart.RoundBox({Side*.76,.96,-1.75},{.34,.065,.031},.018,0xf56b54,M::Glow,5);
        for(int K=0;K<3;++K)Kart.RoundBox({Side*(.29+K*.23),.37,-1.87},{.023,.25,.40},.009,Carbon,M::Surface,4);
    }
    Kart.Loft({},{{-1.94,1.16,1.60,.022},{-1.80,1.33,1.59,.062},{-1.65,1.33,1.55,.064},{-1.50,1.25,1.53,.028}},0xffffff,M::Paint,20,40);
    for(int Side:{-1,1})
    {
        Kart.RoundBox({Side*1.30,1.64,-1.69},{.063,.36,.59},.028,Carbon,M::Surface,6);
        Kart.RoundBox({Side*1.338,1.73,-1.71},{.014,.035,.33},.007,Ivory,M::Surface,4);
        Kart.Tube({{Side*.52,.80,-.69},{Side*.63,1.45,-.72},{Side*.50,1.86,-.76},{0,1.96,-.77}},.047,Carbon,M::Surface,28,12);
    }
    Kart.RoundBox({0,1.17,-.40},{1.08,.93,.60},.19,Carbon,M::Surface,12);
    Kart.RoundBox({0,1.37,-.61},{.69,.60,.14},.065,Graphite,M::Rubber,8);
    Kart.RoundBox({0,.98,-.08},{.79,.15,.69},.07,Graphite,M::Rubber,8);
    for(int Side:{-1,1})
    {
        Kart.Tube({{Side*.41,1.09,.14},{Side*.52,1.29,-.17},{Side*.51,1.58,-.43}},.075,Graphite,M::Rubber,22,12);
        Kart.RoundBox({Side*.20,1.57,-.51},{.125,.080,.17},.036,Carbon,M::Rubber,6);
    }
    Kart.Ellipsoid({0,1.46,-.22},{.38,.43,.30},0xf0eee7,36,24,M::Surface);
    Kart.Ellipsoid({0,1.72,-.24},{.44,.18,.28},Graphite,32,16,M::Surface);
    Kart.Cylinder({0,1.77,-.24},{0,1.92,-.24},.22,.20,Carbon,32,M::Rubber);
    for(int Side:{-1,1})
    {
        Kart.Tube({{Side*.30,1.66,-.11},{Side*.46,1.50,.01},{Side*.48,1.39,.28},{Side*.36,1.48,.54}},.113,Ivory,M::Surface,30,14);
        Kart.Ellipsoid({Side*.39,1.64,-.12},{.15,.16,.19},0xffffff,24,16,M::Paint);
        Kart.Ellipsoid({Side*.475,1.41,.26},{.13,.13,.16},Graphite,24,16,M::Surface);
        Kart.Ellipsoid({Side*.36,1.48,.56},{.12,.11,.16},Ivory,24,16,M::Surface);
        Kart.RoundBox({Side*.36,1.535,.57},{.14,.055,.14},.025,Graphite,M::Rubber,6);
        Kart.Tube({{Side*.19,1.18,-.09},{Side*.28,.98,.35},{Side*.34,.72,.83}},.15,Graphite,M::Surface,24,16);
        Kart.RoundBox({Side*.34,.69,1.02},{.28,.23,.53},.085,Ivory,M::Surface,8);
        Kart.RoundBox({Side*.34,.58,1.05},{.29,.046,.54},.02,Carbon,M::Rubber,5);
        Kart.Tube({{Side*.22,1.78,-.14},{Side*.19,1.54,.075},{Side*.12,1.27,.084}},.044,0xffffff,M::Paint,24,8);
        Kart.RoundBox({Side*.14,1.25,.085},{.13,.17,.045},.013,Titanium,M::Metal,5);
    }
    Kart.RoundBox({0,1.23,.124},{.16,.13,.046},.024,Gold,M::Metal,6);
    Kart.Cylinder({0,.78,.75},{0,1.43,.56},.035,.035,Silver,16,M::Metal);
    Kart.RoundBox({0,1.09,.74},{.39,.14,.16},.045,Carbon,M::Surface,7);
    Kart.RoundBox({0,1.14,.665},{.26,.056,.024},.009,Cyan,M::Glow,5);
    Kart.Ellipsoid({0,2.21,-.24},{.565,.60,.55},Ivory,64,40,M::Surface);
    Kart.ShellPatch({0,2.21,-.24},{.578,.612,.563},-.96,.96,1.09,1.83,Carbon,M::Rubber,48,20);
    Kart.ShellPatch({0,2.21,-.24},{.584,.617,.570},-.89,.89,1.16,1.74,0x466777,M::Visor,56,24);
    Kart.ShellPatch({0,2.21,-.24},{.588,.621,.574},-.89,.89,1.16,1.183,Titanium,M::Metal,48,2);
    Kart.ShellPatch({0,2.21,-.24},{.571,.607,.557},-.115,.115,.05,1.03,0xffffff,M::Paint,10,26);
    Kart.ShellPatch({0,2.21,-.24},{.573,.609,.559},3.00,3.28,.05,2.20,0xffffff,M::Paint,12,32);
    Kart.RoundBox({0,1.994,.192},{.32,.105,.075},.035,Carbon,M::Rubber,7);
    for(int Side:{-1,1})
    {
        Kart.Cylinder({Side*.531,2.225,-.14},{Side*.565,2.225,-.14},.091,.085,Graphite,32,M::Metal);
        Kart.Cylinder({Side*.566,2.225,-.14},{Side*.577,2.225,-.14},.047,.047,Silver,24,M::Metal);
        Kart.Cylinder({Side*.578,2.225,-.14},{Side*.584,2.225,-.14},.015,.015,Gold,12,M::Metal);
        Kart.ShellPatch({0,2.21,-.24},{.574,.610,.560},Side>0?.34:-.45,Side>0?.45:-.34,.45,.80,Graphite,M::Rubber,5,8);
        Kart.Tube({{Side*.28,1.975,.17},{Side*.37,1.99,.13},{Side*.42,2.02,.08}},.016,Graphite,M::Surface,12,6);
    }
    for(int I=-1;I<=1;++I)Kart.RoundBox({I*.077,1.996,.236},{.024,.043,.012},.005,Titanium,M::Metal,4);
    const V SteeringAxis{0,.66,.75};
    Steering.Torus({},SteeringAxis,.335,.047,Carbon,48,12,M::Rubber);
    const V SteeringU{1,0,0},SteeringV=SteeringAxis.Normal().Cross(SteeringU);
    for(int K=0;K<3;++K)
    {
        const double A=K*Tau/3+.52;const V D=SteeringU*std::cos(A)+SteeringV*std::sin(A);
        Steering.Cylinder(D*.07,D*.29,.025,.019,Titanium,10,M::Metal);
    }
    Steering.Cylinder(SteeringAxis*-.025,SteeringAxis*.028,.09,.09,Gold,24,M::Metal);
    for(int Side:{-1,1})Steering.Ellipsoid(SteeringU*(Side*.24)+SteeringAxis*.038,{.035,.027,.035},Side<0?0xef734d:Cyan,12,8,M::Glow);
    Wheel.Lathe({{-.27,.355},{-.305,.405},{-.305,.47},{-.28,.56},{-.22,.62},{-.14,.65},{.14,.65},{.22,.62},{.28,.56},{.305,.47},{.305,.405},{.27,.355}},0x252b32,M::Rubber,80);
    for(int Side:{-1,1})
    {
        Wheel.Torus({Side*.306,0,0},{1,0,0},.371,.024,Silver,56,8,M::Metal);
        Wheel.Torus({Side*.285,0,0},{1,0,0},.45,.010,Graphite,56,6,M::Rubber);
        Wheel.Torus({Side*.282,0,0},{1,0,0},.516,.009,0x454b50,56,6,M::Rubber);
        Wheel.Cylinder({Side*.19,0,0},{Side*.211,0,0},.278,.278,Graphite,40,M::Metal);
        Wheel.Cylinder({Side*.272,0,0},{Side*.321,0,0},.109,.100,Gold,20,M::Metal);
        Wheel.Cylinder({Side*.322,0,0},{Side*.338,0,0},.044,.044,Carbon,6,M::Metal);
        for(int K=0;K<6;++K)
        {
            const double A=K*Tau/6;const V D{0,std::cos(A),std::sin(A)},E{0,std::cos(A+.16),std::sin(A+.16)};
            Wheel.Cylinder(V{Side*.299,0,0}+D*.096,V{Side*.289,0,0}+E*.335,.029,.042,Silver,10,M::Metal);
            Wheel.Cylinder(V{Side*.326,0,0}+D*.075,V{Side*.339,0,0}+D*.075,.012,.012,Titanium,8,M::Metal);
            for(int J=0;J<2;++J)
            {
                const double A2=A+.25+J*.14;const V Q{Side*.218,std::cos(A2)*.225,std::sin(A2)*.225};
                Wheel.Cylinder(Q,Q+V{Side*.002,0,0},.014,.014,Carbon,8,M::Rubber);
            }
        }
        for(int K=0;K<8;++K)
        {
            const double A=K*.049+.3;const V A0{Side*.311,std::cos(A)*.448,std::sin(A)*.448},B0{Side*.310,std::cos(A)*.499,std::sin(A)*.499};
            Wheel.Cylinder(A0,B0,.009,.009,Ivory,6,M::Surface);
        }
        Wheel.Cylinder({Side*.30,.315,0},{Side*.32,.331,0},.012,.012,Gold,8,M::Metal);
    }
    for(int K=0;K<32;++K)
    {
        const double A=K*Tau/32;TArray<V> Groove;
        for(int J=0;J<5;++J){const double X=-.205+J*.1025,T=A+X*.28,R=.651-std::max(0.0,std::abs(X)-.14)*.28;Groove.Add({X,std::cos(T)*R,std::sin(T)*R});}
        Wheel.Tube(Groove,.0075,0x0c1117,M::Rubber,8,5);
    }
    Output.Add(TEXT("SM_Kart"),MoveTemp(Kart.Mesh));Output.Add(TEXT("SM_KartWheel"),MoveTemp(Wheel.Mesh));Output.Add(TEXT("SM_KartSteering"),MoveTemp(Steering.Mesh));
    for(FName Name:{FName(TEXT("SM_Kart")),FName(TEXT("SM_KartWheel")),FName(TEXT("SM_KartSteering"))})PrepareMeshForUnreal(Output[Name]);
}

void BuildAssets(TMap<FName,FMesh>& Output)
{
    Overdrive::Track Track;Overdrive::Random Rng(91);
    FBuilder Road,Islands,Scenery,Clouds;
    constexpr uint32 Orange=0xff8051,White=0xf4efe4,Cyan=0x83ede5,Dark=0x16243b;
    const auto P=[&](double D,double L,double Lift=0.0){return Track.Position(D,L,Lift);};
    const auto Band=[&](double D,double Next,double A,double B,double Lift,uint32 RGB,M Material=M::Surface)
    { Road.Quad(P(D,A,Lift),P(Next,A,Lift),P(Next,B,Lift),P(D,B,Lift),RGB,Material,{0,1,0}); };
    for(std::size_t I=0;I+1<Track.Samples.size();++I)
    {
        const double D=Track.Samples[I].Distance,Next=Track.Samples[I+1].Distance;
        Band(D,Next,-8.1,8.1,-.28,0x8194a8);Band(D,Next,-7.5,7.5,0,0x536780);
        for(int Side:{-1,1})
        {
            Band(D,Next,Side*6.55,Side*6.68,.035,White);
            Band(D,Next,Side*7.1,Side*7.52,.04,static_cast<int>(D/3)%2?Orange:White);
            Road.Quad(P(D,Side*8,-.3),P(Next,Side*8,-.3),P(Next,Side*8,.55),P(D,Side*8,.55),White);
            Road.Quad(P(D,Side*7.7,.4),P(Next,Side*7.7,.4),P(Next,Side*7.7,1.15),P(D,Side*7.7,1.15),0x99c6cb);
            Band(D,Next,Side*7.65,Side*7.83,1.17,Cyan,M::Glow);
        }
        if(static_cast<int>(D/5)%3==0)Band(D,Next,-.07,.07,.025,0xb7c9d4);
    }
    for(double D=0;D<Track.Length;D+=13)
    {
        for(int Side:{-1,1})
        {
            const V B=P(D,Side*7.8,.15);Road.Cylinder(B,B+V{0,1.6,0},.10,.075,White,8);
            Road.Ellipsoid(B+V{0,1.67,0},{.15,.15,.15},Cyan,10,6,M::Glow);
        }
        if(static_cast<int>(D/13)%3==0){const V B=P(D,0,-1);Road.Cylinder(B,{B.X,-12,B.Z},1.1,.55,0xd7d0c3,12);}
    }
    for(int Lane=0;Lane<12;++Lane)for(int Row=0;Row<3;++Row)
        Band(19+Row*.75,19+(Row+1)*.75,-7+Lane*14.0/12,-7+(Lane+1)*14.0/12,.07,(Lane+Row)%2?White:Dark);
    for(int Side:{-1,1})
    {
        const V B=P(22,Side*8.5,-1);Road.Cylinder(B,B+V{0,10,0},.57,.4,White,16);
        Road.Cylinder(B+V{0,3,0},B+V{0,8.7,0},.59,.42,Orange,16);
    }
    Road.Cylinder(P(22,-9,9),P(22,9,9),.8,.8,Orange,16);
    for(double D:Track.BoostPads)for(int I=0;I<6;++I)
    {
        const double D0=D+I*.75;
        Road.Quad(P(D0,-4,.07),P(D0+1.35,0,.07),P(D0+.52,0,.07),P(D0-.83,-4,.07),Cyan,M::Glow,{0,1,0});
        Road.Quad(P(D0+1.35,0,.07),P(D0,4,.07),P(D0-.83,4,.07),P(D0+.52,0,.07),Cyan,M::Glow,{0,1,0});
    }
    const auto Palm=[&](V Base,double Height,double Angle)
    {
        const V Top=Base+V{std::cos(Angle)*Height*.15,Height,std::sin(Angle)*Height*.15};
        for(int I=0;I<7;++I)Scenery.Cylinder(Base+(Top-Base)*(I/7.0),Base+(Top-Base)*((I+1)/7.0),.3*(1-I/12.0),.3*(1-(I+1)/12.0),I%2?0xac7d67:0xbf9474,10);
        for(int J=0;J<7;++J)
        {
            const double A=J/7.0*Tau+Angle,Len=Height*(.43+Rng.Next()*.16);
            const V Dir{std::cos(A),0,std::sin(A)},Side{-std::sin(A),0,std::cos(A)};
            const auto Center=[&](double T){return Top+Dir*(Len*T)+V{0,std::sin(T*Overdrive::Pi)*Len*.21-T*T*Len*.4,0};};
            for(int K=0;K<5;++K)
            {
                const double T=K/5.0,U=(K+1)/5.0,W=std::sin(T*Overdrive::Pi)*Len*.16,WW=std::sin(U*Overdrive::Pi)*Len*.16;
                const V C=Center(T),CC=Center(U);
                Scenery.Quad(C-Side*W,CC-Side*WW,CC+V{0,.13,0},C+V{0,.13,0},J%2?0x389783:0x4dbd91);
                Scenery.Quad(C+V{0,.13,0},CC+V{0,.13,0},CC+Side*WW,C+Side*W,0x72c99a);
            }
        }
        for(int I=0;I<3;++I)Scenery.Ellipsoid(Top+V{std::cos(I*2.0)*.35,-.35,std::sin(I*2.0)*.35},{.28,.32,.28},0x876950,10,7);
    };
    const V Centers[]={{-77,17,91},{57,22,79},{100,31,12},{50,40,-76},{-66,26,-76},{-85,19,-11},{-12,22,24},{166,17,-117},{-157,23,-163},{191,6,92}};
    const double Radii[]={28,40,30,43,45,30,29,35,49,22},Depths[]={39,49,51,64,50,40,39,60,70,46};
    for(int Island=0;Island<10;++Island)
    {
        const V Center=Centers[Island];const double Radius=Radii[Island],Depth=Depths[Island];
        double Ring[18];for(double& R:Ring)R=.83+Rng.Next()*.24;
        const auto Vertex=[&](int I,int Level)
        {
            const double A=I/18.0*Tau,R=Radius*Ring[I%18]*std::array<double,4>{1,.97,.57,.12}[Level];
            return Center+V{std::cos(A)*R,std::array<double,4>{0,-Depth*.2,-Depth*.62,-Depth}[Level],std::sin(A)*R};
        };
        for(int I=0;I<18;++I)
        {
            Islands.Tri(Center,Vertex(I+1,0),Vertex(I,0),I%3?0x49ad86:0x65bc90,M::Surface,{0,1,0});
            for(int L=0;L<3;++L)Islands.Quad(Vertex(I,L),Vertex(I+1,L),Vertex(I+1,L+1),Vertex(I,L+1),(I+L)%3?0xa29291:0x778ba4);
            const V A=Vertex(I,0),B=Vertex(I+1,0);Islands.Quad(A,B,B+V{0,-1.8,0},A+V{0,-1.8,0},0xead5ac);
        }
        for(int I=0;I<13;++I)
        {
            const double A=Rng.Next()*Tau,R=Radius*(.42+Rng.Next()*.39);
            const V Base=Center+V{std::cos(A)*R,.2,std::sin(A)*R};
            double Nearest=1.e9;for(const auto& S:Track.Samples)Nearest=std::min(Nearest,std::hypot(S.Position.X-Base.X,S.Position.Z-Base.Z));
            const double PalmHeight=9+Rng.Next()*7;
            if(Nearest>Overdrive::TrackWidth*.5+PalmHeight*.75+6)Palm(Base,PalmHeight,A);
            if(Nearest>14)for(int K=0;K<3;++K)
            {
                const V Offset{std::cos(K*1.8)*2.3,.8,std::sin(K*1.8)*2.3};
                Scenery.Ellipsoid(Base+Offset,{1.8,1.35,1.6},K%2?0x288e77:0x5ab991,14,9);
                Scenery.Ellipsoid(Base+Offset+V{.6,1.25,.3},{.3,.25,.3},0xffc18c,10,7);
            }
        }
        for(int I=0;I<4;++I)
        {
            const double A=Rng.Next()*Tau,R=Radius*(.95+Rng.Next()*.15);
            Islands.Ellipsoid(Center+V{std::cos(A)*R,-Depth*.6-Rng.Next()*12,std::sin(A)*R},{2+Rng.Next()*3,3+Rng.Next()*4,2+Rng.Next()*3},0x969fad,10,7);
        }
        if(Island==2||Island==4||Island==7)
        {
            const V Waterfall=Center+V{Radius*.72,-1,Radius*.3};
            for(int K=0;K<5;++K)
            {
                const double X=Waterfall.X+K*.62;
                Scenery.Quad({X,Waterfall.Y,Waterfall.Z},{X+.6,Waterfall.Y,Waterfall.Z},{X+.6,-37,Waterfall.Z+5},{X,-37,Waterfall.Z+5},K%2?0xa1eee7:0x67c7d1);
            }
            Scenery.Ellipsoid({Waterfall.X+1.5,-37,Waterfall.Z+5},{7,.25,5},0xb7e1db,20,9);
        }
    }
    const V Tower{55,40,-83};
    Scenery.Cylinder(Tower,Tower+V{0,34,0},3,2.2,0xf4e7d0,24);
    for(int I=0;I<3;++I)Scenery.Cylinder(Tower+V{0,8.0+I*8,0},Tower+V{0,10.0+I*8,0},2.83-I*.19,2.78-I*.19,Orange,24);
    Scenery.Cylinder(Tower+V{0,33,0},Tower+V{0,35,0},4.2,4.2,0x344962,24);
    Scenery.Cylinder(Tower+V{0,35,0},Tower+V{0,40,0},2.7,2.7,Cyan,24,M::Glow);
    Scenery.Cylinder(Tower+V{0,40,0},Tower+V{0,44,0},4.2,0,Orange,24);
    Scenery.Torus(Tower+V{0,21,0},{.12,1,.24},12,.42,Orange,72);
    Scenery.Torus(Tower+V{0,24,0},{-.3,1,.13},16,.19,Cyan,72,8,M::Glow);
    for(int I=0;I<18;++I)
    {
        const double A=I/18.0*Tau,R=300+Rng.Next()*180;const V C{std::cos(A)*R,Rng.Next()*40-22,std::sin(A)*R};
        for(int J=0;J<5;++J)Clouds.Ellipsoid(C+V{(J-2)*12.0,Rng.Next()*6,Rng.Next()*10},{20+Rng.Next()*10,8+Rng.Next()*8,15+Rng.Next()*12},0xf0eee6,20,12);
    }
    const auto Balloon=[&](V C,double Size,uint32 RGB)
    {
        const auto Point=[&](int I,int J){const double T=J/16.0*Overdrive::Pi,R=std::sin(T)*(1+std::cos(T)*.16)*Size;return C+V{std::cos(I/24.0*Tau)*R,std::cos(T)*Size*1.2,std::sin(I/24.0*Tau)*R};};
        for(int J=0;J<16;++J)for(int I=0;I<24;++I)Scenery.Quad(Point(I,J),Point(I+1,J),Point(I+1,J+1),Point(I,J+1),(I/3)%2?RGB:0xffedcb);
        const V Bottom=C+V{0,-Size*1.75,0};Scenery.BevelBox(Bottom,{Size*.42,Size*.3,Size*.42},.2,0xb98e69);
        for(int X:{-1,1})for(int Z:{-1,1})Scenery.Cylinder(Bottom+V{X*Size*.18,0,Z*Size*.18},C+V{X*Size*.2,-Size,Z*Size*.2},.065,.065,0xe2d6b7,8);
    };
    Balloon({111,91,-85},9,Orange);Balloon({-138,79,-128},6,0x80b9d9);Balloon({192,69,28},7,0xeec26e);
    for(int I=0;I<20;++I)
    {
        const double D=I/20.0*Track.Length;const auto S=Track.At(D);const V B=S.Position+S.Right*(I%2?10:-10);
        Scenery.Cylinder(B,B+V{0,5.2,0},.08,.08,White,8);
        Scenery.Tri(B+V{0,5.2,0},B+V{0,3.7,0},B+S.Tangent*2.4+V{0,4.6,0},I%2?Orange:Cyan);
    }
    Output.Add(TEXT("SM_Road"),MoveTemp(Road.Mesh));Output.Add(TEXT("SM_Islands"),MoveTemp(Islands.Mesh));
    Output.Add(TEXT("SM_Scenery"),MoveTemp(Scenery.Mesh));Output.Add(TEXT("SM_Clouds"),MoveTemp(Clouds.Mesh));

    FBuilder Crystal;Crystal.Diamond({},.69,0x8ceadb);Crystal.Torus({},{0,1,0},1.03,.045,0xddffff,32,8,M::Glow);Output.Add(TEXT("SM_Crystal"),MoveTemp(Crystal.Mesh));
    FBuilder Coin;Coin.Cylinder({0,0,-.07},{0,0,.07},.43,.43,0xffd479,28,M::Metal);Coin.Torus({0,0,.08},{0,0,1},.34,.04,0xffe4a4,28,8);Coin.Box({0,0,.12},{.07,.35,.02},0xfff3b8);Output.Add(TEXT("SM_Coin"),MoveTemp(Coin.Mesh));
    FBuilder Exhaust;for(int Side:{-1,1})Exhaust.Ellipsoid({Side*.65,.76,-2.55},{.16,.14,.8},0x87eeff,16,10,M::Glow);Output.Add(TEXT("SM_Exhaust"),MoveTemp(Exhaust.Mesh));
    FBuilder Shield;Shield.Torus({0,1.1,0},{0,1,0},2.25,.04,Cyan,60,8,M::Glow);Shield.Torus({0,1.1,0},{1,.4,0},2.1,.025,0xb9fbfa,60,8,M::Glow);Output.Add(TEXT("SM_Shield"),MoveTemp(Shield.Mesh));
    FBuilder Sparks;for(int Side:{-1,1})for(int I=0;I<5;++I)Sparks.Diamond({Side*(1.26+I*.12),.27+I*.1,-1.6-I*.35},.055+I*.012,I%2?0xffb06c:Cyan);Output.Add(TEXT("SM_Sparks"),MoveTemp(Sparks.Mesh));
    FBuilder Ocean;Ocean.Quad({-1800,-38,-1800},{-1800,-38,1800},{1800,-38,1800},{1800,-38,-1800},0xffffff,M::Surface,{0,1,0});Output.Add(TEXT("SM_Ocean"),MoveTemp(Ocean.Mesh));
    for(auto& Entry:Output)PrepareMeshForUnreal(Entry.Value);
    BuildKartAssets(Output);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverdrivePrimitiveNormalsTest,"Overdrive.Graphics.PrimitiveOutwardNormals",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOverdrivePrimitiveNormalsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const V Center{3,5,-2};
    const FVector3f UnrealCenter(ToUnreal(Center));
    FBuilder Beveled,Diamond;
    Beveled.BevelBox(Center,{4,2,6},.35,0xffffff);
    Diamond.Diamond(Center,1,0xffffff);
    FBuilder* Builders[]={&Beveled,&Diamond};
    const TCHAR* Names[]={TEXT("Beveled box"),TEXT("Diamond")};
    for(int32 Index=0;Index<2;++Index)
    {
        PrepareMeshForUnreal(Builders[Index]->Mesh);
        int32 InwardNormals=0,InwardFaces=0,Triangles=0;
        for(const FSection& Section:Builders[Index]->Mesh.Sections)
        {
            for(int32 I=0;I+2<Section.Vertices.Num();I+=3)
            {
                const FVertex& A=Section.Vertices[I];const FVertex& B=Section.Vertices[I+1];const FVertex& C=Section.Vertices[I+2];
                const FVector3f Outward=(A.Position+B.Position+C.Position)/3.f-UnrealCenter;
                if(FVector3f::DotProduct(A.Normal+B.Normal+C.Normal,Outward)<=0)++InwardNormals;
                if(FVector3f::DotProduct(FVector3f::CrossProduct(C.Position-A.Position,B.Position-A.Position),Outward)<=0)++InwardFaces;
                ++Triangles;
            }
        }
        TestTrue(*FString::Printf(TEXT("%s contains triangles"),Names[Index]),Triangles>0);
        TestEqual(*FString::Printf(TEXT("%s normals point away from its center"),Names[Index]),InwardNormals,0);
        TestEqual(*FString::Printf(TEXT("%s exterior faces point away from its center"),Names[Index]),InwardFaces,0);
    }
    return true;
}
#endif
}
