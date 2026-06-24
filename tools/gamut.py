import numpy as np

def npm(p, w):
    # p: dict primaries xy; w: white xy -> RGB(linear)->XYZ matrix
    xr,yr=p['R']; xg,yg=p['G']; xb,yb=p['B']; xw,yw=w
    def XYZ(x,y): return np.array([x/y,1.0,(1-x-y)/y])
    Xr,Xg,Xb=XYZ(xr,yr),XYZ(xg,yg),XYZ(xb,yb)
    M=np.array([Xr,Xg,Xb]).T
    W=XYZ(xw,yw)
    S=np.linalg.solve(M,W)
    return M*S  # columns scaled

D65=(0.3127,0.3290)
rec709={'R':(0.64,0.33),'G':(0.30,0.60),'B':(0.15,0.06)}
spaces={
 'SGamut3Cine':{'R':(0.766,0.275),'G':(0.225,0.800),'B':(0.089,-0.087)},
 'SGamut3':{'R':(0.730,0.280),'G':(0.140,0.855),'B':(0.100,-0.050)},
 'AWG3':{'R':(0.684,0.313),'G':(0.221,0.848),'B':(0.0861,-0.102)},
 'AWG4':{'R':(0.7347,0.2653),'G':(0.1424,0.8576),'B':(0.0991,-0.0308)},
 'VGamut':{'R':(0.730,0.280),'G':(0.165,0.840),'B':(0.100,-0.030)},
 'REDWideGamut':{'R':(0.780308,0.304253),'G':(0.121595,1.493994),'B':(0.095612,-0.084589)},
 'BMDWGGen5':{'R':(0.7177,0.3171),'G':(0.2280,0.8616),'B':(0.1006,-0.0820)},
 'DGamut':{'R':(0.71,0.31),'G':(0.21,0.88),'B':(0.09,-0.08)},
 'CinemaGamut':{'R':(0.740,0.270),'G':(0.170,1.140),'B':(0.080,-0.100)},
 'Rec2020':{'R':(0.708,0.292),'G':(0.170,0.797),'B':(0.131,0.046)},
}
M709=npm(rec709,D65)
M709inv=np.linalg.inv(M709)
for name,p in spaces.items():
    Msrc=npm(p,D65)
    M=M709inv@Msrc   # src linear RGB -> rec709 linear RGB (both D65)
    rows=", ".join("%.7ff"%v for v in M.flatten())
    print(f"// {name} -> Rec709 linear")
    print(f"{{ {rows} }},")
