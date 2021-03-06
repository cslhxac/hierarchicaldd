//#####################################################################
// Copyright 2002-2009, Doug Enright, Ron Fedkiw, Eran Guendelman, Geoffrey Irving, Sergey Koltakov, Michael Lentine, Neil Molino, Andrew Selle, Eftychios Sifakis, Rachel Weinstein.
// This file is part of PhysBAM whose distribution is governed by the license contained in the accompanying file PHYSBAM_COPYRIGHT.txt.
//#####################################################################
// Class MULTIBODY_LEVELSET_IMPLICIT_OBJECT
//#####################################################################
#ifndef __MULTIBODY_LEVELSET_IMPLICIT_OBJECT__
#define __MULTIBODY_LEVELSET_IMPLICIT_OBJECT__

#include <PhysBAM_Tools/Interpolation/INTERPOLATION_FORWARD.h>
#include <PhysBAM_Tools/Matrices/MATRIX_3X3.h>
#include <PhysBAM_Tools/Matrices/MATRIX_POLICY.h>
#include <PhysBAM_Geometry/Basic_Geometry/BASIC_GEOMETRY_POLICY.h>
#include <PhysBAM_Tools/Data_Structures/HASHTABLE.h>
#include <PhysBAM_Geometry/Collision_Detection/COLLISION_GEOMETRY_SPATIAL_PARTITION.h>
#include <PhysBAM_Geometry/Collisions/COLLISION_GEOMETRY_COLLECTION.h>
#include <PhysBAM_Geometry/Collisions/COLLISION_GEOMETRY_ID.h>
#include <PhysBAM_Geometry/Grids_Uniform_Level_Sets/LEVELSET_1D.h>
#include <PhysBAM_Geometry/Grids_Uniform_Level_Sets/LEVELSET_2D.h>
#include <PhysBAM_Geometry/Grids_Uniform_Level_Sets/LEVELSET_3D.h>
#include <PhysBAM_Geometry/Grids_Uniform_Level_Sets/LEVELSET_POLICY_UNIFORM.h>
#include <PhysBAM_Geometry/Implicit_Objects/IMPLICIT_OBJECT.h>
#include <PhysBAM_Geometry/Implicit_Objects/IMPLICIT_OBJECT_TRANSFORMED.h>
#include <PhysBAM_Geometry/Solids_Geometry/RIGID_GEOMETRY_COLLECTION.h>
namespace PhysBAM{

template<class TV> class GRID;

template<class TV>
class MULTIBODY_LEVELSET_IMPLICIT_OBJECT:public IMPLICIT_OBJECT<TV>
{
    typedef typename TV::SCALAR T;
    typedef typename TV::SPIN T_SPIN;
    typedef typename GRID_ARRAYS_POLICY<GRID<TV> >::ARRAYS_SCALAR T_ARRAYS_SCALAR;
    typedef typename REBIND<T_ARRAYS_SCALAR,TV>::TYPE T_ARRAYS_VECTOR;
    typedef typename LEVELSET_POLICY<GRID<TV> >::LEVELSET T_LEVELSET;
    typedef typename BASIC_GEOMETRY_POLICY<TV>::ORIENTED_BOX T_ORIENTED_BOX;
public:
    typedef IMPLICIT_OBJECT<TV> BASE;
    using BASE::box;

    ARRAY<IMPLICIT_OBJECT<TV>*>* levelsets;
    RIGID_GEOMETRY_COLLECTION<TV> *rigid_geometry_collection; //for collision body spatial partition

    mutable OPERATION_HASH<COLLISION_GEOMETRY_ID> get_potential_collisions_already_added;

protected:
    T minimum_cell_size;
public:
    bool need_destroy_data;
    bool use_intersection;

    MULTIBODY_LEVELSET_IMPLICIT_OBJECT(ARRAY<IMPLICIT_OBJECT<TV>*>* levelsets_input)
        :levelsets(levelsets_input),rigid_geometry_collection(0),need_destroy_data(false),use_intersection(false)
    {
        Update_Box();Update_Minimum_Cell_Size();
    }

    virtual ~MULTIBODY_LEVELSET_IMPLICIT_OBJECT()
    {
        if(need_destroy_data) Delete_Pointers_And_Clean_Memory();
        if(rigid_geometry_collection) delete rigid_geometry_collection;
    }

    static MULTIBODY_LEVELSET_IMPLICIT_OBJECT* Create()
    {MULTIBODY_LEVELSET_IMPLICIT_OBJECT<TV>* object=new MULTIBODY_LEVELSET_IMPLICIT_OBJECT<TV>(new ARRAY<IMPLICIT_OBJECT<TV>*>);
    object->need_destroy_data=true;return object;}

    void Delete_Pointers_And_Clean_Memory()
    {for(int i=1;i<=levelsets->m;i++) delete (*levelsets)(i);delete levelsets;}

    void Set_Custom_Interpolation(INTERPOLATION_UNIFORM<T,T,GRID<TV> >& interpolation_input)
    {for(int i=1;i<=levelsets->m;i++) (*levelsets)(i)->Set_Custom_Interpolation(interpolation_input);}

    void Set_Custom_Secondary_Interpolation(INTERPOLATION_UNIFORM<T,T,GRID<TV> >& interpolation_input)
    {for(int i=1;i<=levelsets->m;i++) (*levelsets)(i)->Set_Custom_Secondary_Interpolation(interpolation_input);}

    void Set_Custom_Normal_Interpolation(INTERPOLATION_UNIFORM<TV,GRID<TV> >& interpolation_input)
    {for(int i=1;i<=levelsets->m;i++) (*levelsets)(i)->Set_Custom_Normal_Interpolation(interpolation_input);}

    void Update_Box() PHYSBAM_OVERRIDE
    {box=RANGE<TV>::Empty_Box();
    for(int i=1;i<=levelsets->m;i++){(*levelsets)(i)->Update_Box();box.Enlarge_To_Include_Box((*levelsets)(i)->box);}}

    void Update_Minimum_Cell_Size(const int maximum_depth=0) PHYSBAM_OVERRIDE
    {minimum_cell_size=(T)FLT_MAX;
    for(int i=1;i<=levelsets->m;i++){
        (*levelsets)(i)->Update_Minimum_Cell_Size(maximum_depth);
        minimum_cell_size=min((*levelsets)(i)->Minimum_Cell_Size(),minimum_cell_size);}}

    T Minimum_Cell_Size_Within_Box(const RANGE<TV>& box) const  PHYSBAM_OVERRIDE 
    {T minimum_edge_length=(T)FLT_MAX;for(int i=1;i<=levelsets->m;i++) minimum_edge_length=min((*levelsets)(i)->Minimum_Cell_Size_Within_Box(box),minimum_edge_length);
    return minimum_edge_length;} // TODO: make this check overlap with the grids.

    T operator()(const TV& location) const PHYSBAM_OVERRIDE
    {T phi_value=(T)FLT_MAX;
    ARRAY<IMPLICIT_OBJECT<TV>*>* local_levelsets=levelsets;
    if(rigid_geometry_collection) local_levelsets=Get_Potential_Levelsets(location);
    for(int i=1;i<=local_levelsets->m;i++){
        if((*local_levelsets)(i)->box.Lazy_Outside(location)) phi_value=use_intersection?max((*local_levelsets)(i)->Extended_Phi(location),phi_value):min((*local_levelsets)(i)->Extended_Phi(location),phi_value);
        else phi_value=use_intersection?max((*(*local_levelsets)(i))(location),phi_value):min((*(*local_levelsets)(i))(location),phi_value);}
    return phi_value;}

    T Phi_With_Index(const TV& location,int& levelset_index) const
    {T phi_value=(T)FLT_MAX;T new_phi_value;
    ARRAY<IMPLICIT_OBJECT<TV>*>* local_levelsets=levelsets;
    if(rigid_geometry_collection) local_levelsets=Get_Potential_Levelsets(location);
    for(int i=1;i<=local_levelsets->m;i++){
        if((*local_levelsets)(i)->box.Lazy_Outside(location)) new_phi_value=(*local_levelsets)(i)->Extended_Phi(location);
        else new_phi_value=(*(*local_levelsets)(i))(location);
        if(new_phi_value<phi_value){phi_value=new_phi_value;levelset_index=i;}}
    return phi_value;}

    T Extended_Phi(const TV& location) const PHYSBAM_OVERRIDE
    {T phi_value=(T)FLT_MAX;
    ARRAY<IMPLICIT_OBJECT<TV>*>* local_levelsets=levelsets;
    if(rigid_geometry_collection) local_levelsets=Get_Potential_Levelsets(location);
    for(int i=1;i<=local_levelsets->m;i++) phi_value=use_intersection?max((*local_levelsets)(i)->Extended_Phi(location),phi_value):min((*local_levelsets)(i)->Extended_Phi(location),phi_value);
    return phi_value;}

    T Phi_Secondary(const TV& location) const PHYSBAM_OVERRIDE
    {PHYSBAM_NOT_IMPLEMENTED();} // TODO: implement this, but need Phi_Secondary_Extended()!

    TV Normal(const TV& location,const int aggregate=-1) const PHYSBAM_OVERRIDE
    {assert((aggregate >= 1 && aggregate <= 2*GRID<TV>::dimension) || aggregate == -1);
    if(aggregate != -1) return box.Normal(aggregate);
    else{
        int index=0;Phi_With_Index(location,index);return (*levelsets)(index)->Normal(location);}}

    TV Extended_Normal(const TV& location,const int aggregate=-1) const PHYSBAM_OVERRIDE
    {assert((aggregate >= 1 && aggregate <= 2*GRID<TV>::dimension) || aggregate == -1);
    if(aggregate != -1) return box.Normal(aggregate);
    else{
        int index=0;Phi_With_Index(location,index);return (*levelsets)(index)->Extended_Normal(location);}}

    void Compute_Normals()  PHYSBAM_OVERRIDE
    {for(int i=1;i<=levelsets->m;i++) (*levelsets)(i)->Compute_Normals();}

    void Compute_Cell_Minimum_And_Maximum(const bool recompute_if_exists=true) PHYSBAM_OVERRIDE
    {for(int i=1;i<=levelsets->m;i++) (*levelsets)(i)->Compute_Cell_Minimum_And_Maximum(recompute_if_exists);}

    void Inflate(const T inflation_distance) PHYSBAM_OVERRIDE
    {for(int i=1;i<=levelsets->m;i++) (*levelsets)(i)->Inflate(inflation_distance);}

    T Integration_Step(const T phi) const PHYSBAM_OVERRIDE
    {T step=FLT_MAX;for(int i=1;i<=levelsets->m;i++) step=min(step,(*levelsets)(i)->Integration_Step(phi));return step;}

    T Minimum_Cell_Size() const  PHYSBAM_OVERRIDE
    {return minimum_cell_size;}

    bool Lazy_Inside(const TV& location,const T contour_value=0) const PHYSBAM_OVERRIDE
    {if(!box.Lazy_Inside(location)) return false;
    ARRAY<IMPLICIT_OBJECT<TV>*>* local_levelsets=levelsets;
    if(rigid_geometry_collection) local_levelsets=Get_Potential_Levelsets(location);
    for(int i=1;i<=local_levelsets->m;i++) if((*local_levelsets)(i)->Lazy_Inside(location,contour_value)) return true;
    return false;}

    bool Lazy_Inside_And_Value(const TV& location,T& phi_value,const T contour_value=0) const PHYSBAM_OVERRIDE
    {if(!box.Lazy_Inside(location)) return false;
    bool levelset_value=false;phi_value=(T)FLT_MAX;T levelset_phi_value=T();
    ARRAY<IMPLICIT_OBJECT<TV>*>* local_levelsets=levelsets;
    if(rigid_geometry_collection) local_levelsets=Get_Potential_Levelsets(location);
    for(int i=1;i<=local_levelsets->m;i++){
        levelset_value=levelset_value||(*local_levelsets)(i)->Lazy_Inside_And_Value(location,levelset_phi_value,contour_value);
        if(levelset_phi_value<phi_value) phi_value=levelset_phi_value;}
    return levelset_value;}

    bool Lazy_Inside_Extended_Levelset(const TV& unclamped_X,const T contour_value=0) const PHYSBAM_OVERRIDE
    {for(int i=1;i<=levelsets->m;i++) if((*levelsets)(i)->Lazy_Inside_Extended_Levelset(unclamped_X,contour_value)) return true;
    return false;}

    bool Lazy_Inside_Extended_Levelset_And_Value(const TV& unclamped_X,T& phi_value,const T contour_value=0) const PHYSBAM_OVERRIDE
    {bool levelset_value=false;phi_value=(T)FLT_MAX;T levelset_phi_value=T();
    for(int i=1;i<=levelsets->m;i++){
        levelset_value=levelset_value||(*levelsets)(i)->Lazy_Inside_Extended_Levelset_And_Value(unclamped_X,levelset_phi_value,contour_value);
        if(levelset_phi_value<phi_value) phi_value=levelset_phi_value;}
    return levelset_value;}

    bool Lazy_Outside(const TV& location,const T contour_value=0) const PHYSBAM_OVERRIDE
    {if(box.Lazy_Outside(location)) return true;
    bool levelset_value=true;for(int i=1;i<=levelsets->m;i++) levelset_value=levelset_value && (*levelsets)(i)->Lazy_Outside(location,contour_value);
    return levelset_value;}

    bool Lazy_Outside_Extended_Levelset(const TV& unclamped_X,const T contour_value=0) const PHYSBAM_OVERRIDE
    {bool levelset_value=true;for(int i=1;i<=levelsets->m;i++) levelset_value=levelset_value && (*levelsets)(i)->Lazy_Outside_Extended_Levelset(unclamped_X,contour_value);
    return levelset_value;}

    bool Lazy_Outside_Extended_Levelset_And_Value(const TV& unclamped_X,T& phi_value,const T contour_value=0) const PHYSBAM_OVERRIDE
    {bool levelset_value=true;phi_value=(T)FLT_MAX;T levelset_phi_value=T();
    for(int i=1;i<=levelsets->m;i++){
        levelset_value=levelset_value && (*levelsets)(i)->Lazy_Outside_Extended_Levelset_And_Value(unclamped_X,levelset_phi_value,contour_value);
        if(levelset_phi_value<phi_value) phi_value=levelset_phi_value;}
    return levelset_value;}

    T Min_Phi() const PHYSBAM_OVERRIDE
    {T min_phi=(T)FLT_MAX;for(int i=1;i<=levelsets->m;i++) min_phi=min(min_phi,(*levelsets)(i)->Min_Phi());
    return min_phi;}

    void Rescale(const T scaling_factor) PHYSBAM_OVERRIDE
    {for(int i=1;i<=levelsets->m;i++) (*levelsets)(i)->Rescale(scaling_factor);
    Update_Box();Update_Minimum_Cell_Size();}

    void Translate(const TV& translation) PHYSBAM_OVERRIDE
    {for(int i=1;i<=levelsets->m;i++) (*levelsets)(i)->Translate(translation);
    Update_Box();Update_Minimum_Cell_Size();}

    VECTOR<T,TV::dimension-1> Principal_Curvatures(const TV& X) const PHYSBAM_OVERRIDE
    {int index=0;Phi_With_Index(X,index);return (*levelsets)(index)->Principal_Curvatures(X);}

    void Create_New_Partition(const SPATIAL_PARTITION_VOXEL_SIZE_HEURISTIC heuristic,const int number_of_boxes,const T voxel_size_scale_factor)
    {if(rigid_geometry_collection) delete rigid_geometry_collection;
    rigid_geometry_collection=new RIGID_GEOMETRY_COLLECTION<TV>(0,new COLLISION_GEOMETRY_COLLECTION<TV>());
    rigid_geometry_collection->particles.array_collection->Resize(levelsets->m);
    for(int i=1;i<=levelsets->m;i++){RIGID_GEOMETRY<TV>* rigid_geometry=new RIGID_GEOMETRY<TV>(*rigid_geometry_collection,true,i);
        FRAME<TV> frame;if(IMPLICIT_OBJECT_TRANSFORMED<TV,FRAME<TV> >* transformed=dynamic_cast<IMPLICIT_OBJECT_TRANSFORMED<TV,FRAME<TV> >*>((*levelsets)(i))) frame=*transformed->transform;
        rigid_geometry->X()=frame.t;rigid_geometry->Rotation()=frame.r;rigid_geometry->V()=TV();rigid_geometry->Angular_Velocity()=T_SPIN();
        rigid_geometry->Add_Structure(*(*levelsets)(i));rigid_geometry->Update_Bounding_Box();}
    rigid_geometry_collection->collision_body_list->Update_Spatial_Partition(heuristic,number_of_boxes,voxel_size_scale_factor);}

    ARRAY<IMPLICIT_OBJECT<TV>*>* Get_Potential_Levelsets(const TV& X) const
    {if(!rigid_geometry_collection) return levelsets;
    ARRAY<IMPLICIT_OBJECT<TV>*>* potential_levelsets=new ARRAY<IMPLICIT_OBJECT<TV>*>;
    ARRAY<COLLISION_GEOMETRY_ID> ids;
    rigid_geometry_collection->collision_body_list->spatial_partition->Get_Potential_Collisions(X,ids,get_potential_collisions_already_added);
    if(!ids.m) for(T thickness=(T)1e-4;!ids.m;thickness*=2){RANGE<TV> box(X);rigid_geometry_collection->collision_body_list->spatial_partition->Get_Potential_Collisions(box.Thickened(thickness),ids,get_potential_collisions_already_added);}
    for(int i=1;i<=ids.m;i++) potential_levelsets->Append((*levelsets)(Value(ids(i))));
    return potential_levelsets;}

    ARRAY<IMPLICIT_OBJECT<TV>*>* Get_Potential_Levelsets(const RANGE<TV>& box) const
    {if(!rigid_geometry_collection) return levelsets;
    ARRAY<IMPLICIT_OBJECT<TV>*>* potential_levelsets=new ARRAY<IMPLICIT_OBJECT<TV>*>;
    ARRAY<COLLISION_GEOMETRY_ID> ids;
    rigid_geometry_collection->collision_body_list->spatial_partition->Get_Potential_Collisions(box,ids,get_potential_collisions_already_added);
    if(!ids.m) for(T thickness=(T)1e-4;!ids.m;thickness*=2) rigid_geometry_collection->collision_body_list->spatial_partition->Get_Potential_Collisions(box.Thickened(thickness),ids,get_potential_collisions_already_added);
    for(int i=1;i<=ids.m;i++) potential_levelsets->Append((*levelsets)(Value(ids(i))));
    return potential_levelsets;}

    virtual std::string Name() const PHYSBAM_OVERRIDE {return Static_Name();}
    static std::string Static_Name()
    {return STRING_UTILITIES::string_sprintf("MULTIBODY_LEVELSET_IMPLICIT_OBJECT<T,VECTOR<T,%d> >",TV::dimension);}

    virtual std::string Extension() const PHYSBAM_OVERRIDE {return Static_Extension();}
    static std::string Static_Extension()
    {return TV::dimension==2?"mphi2d":"mphi";}

//###########################################################################
};

}
#endif
