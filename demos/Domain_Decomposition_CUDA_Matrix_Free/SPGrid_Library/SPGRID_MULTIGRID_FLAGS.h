//#####################################################################
//Removed For Anonymity: Copyright Authors of Submission pap173s1
// Distributed under the FreeBSD license (see license.txt)
//#####################################################################
#ifndef __SPGRID_MULTIGRID_FLAGS_h__
#define __SPGRID_MULTIGRID_FLAGS_h__

namespace PhysBAM{
enum {

    // Cell properties
  
    SPGrid_Solver_Cell_Type_Active    = 0x00010000u, // Set by user (Active = Interior but not Dirichlet)
    SPGrid_Solver_Cell_Type_Dirichlet = 0x00020000u, // Set by user
    SPGrid_Solver_Cell_Type_Boundary  = 0x00040000u, // Generated automatically
    SPGrid_Solver_Cell_Type_Interface = 0x00080000u, // Generated by dd 
                                                  
    // Determined by cell types                   
    SPGrid_Solver_Face_Minus_X_Active = 0x00100000u,
    SPGrid_Solver_Face_Plus_X_Active  = 0x00200000u,
    SPGrid_Solver_Face_Minus_Y_Active = 0x00400000u,
    SPGrid_Solver_Face_Plus_Y_Active  = 0x00800000u,
    SPGrid_Solver_Face_Minus_Z_Active = 0x01000000u,
    SPGrid_Solver_Face_Plus_Z_Active  = 0x02000000u,

    // Least significant halfword reserved for Partition ID

    SPGrid_Solver_PartitionID_Mask    = 0x0000ffffu
};
}

#endif
