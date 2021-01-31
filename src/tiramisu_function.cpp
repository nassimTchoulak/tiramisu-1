#include <isl/ctx.h>
#include <isl/aff.h>
#include <isl/set.h>
#include <isl/map.h>
#include <isl/id.h>
#include <isl/constraint.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/ast_build.h>
#include <isl/flow.h>

#include <tiramisu/debug.h>
#include <tiramisu/core.h>

#include <fstream>

#include <iostream>

namespace tiramisu
{

/**
 * Retrieve the access function of the ISL AST leaf node (which represents a
 * computation). Store the access in computation->access.
 */
isl_ast_node *for_code_generator_after_for(
        isl_ast_node *node, isl_ast_build *build, void *user);

isl_map *isl_map_align_range_dims(isl_map *map, int max_dim)
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    assert(map != NULL);
    int mdim = isl_map_dim(map, isl_dim_out);
    assert(max_dim >= mdim);

    DEBUG(10, tiramisu::str_dump("Input map:", isl_map_to_str(map)));

    const char *original_range_name = isl_map_get_tuple_name(map, isl_dim_out);

    map = isl_map_add_dims(map, isl_dim_out, max_dim - mdim);

    for (int i = mdim; i < max_dim; i++)
    {
        isl_space *sp = isl_map_get_space(map);
        isl_local_space *lsp = isl_local_space_from_space(sp);
        isl_constraint *cst = isl_constraint_alloc_equality(lsp);
        cst = isl_constraint_set_coefficient_si(cst, isl_dim_out, i, 1);
        map = isl_map_add_constraint(map, cst);
    }

    map = isl_map_set_tuple_name(map, isl_dim_out, original_range_name);

    DEBUG(10, tiramisu::str_dump("After alignment, map = ",
                                 isl_map_to_str(map)));

    DEBUG_INDENT(-4);
    return map;
}

/**
  * Add a buffer to the function.
  */
void function::add_buffer(std::pair <std::string, tiramisu::buffer *> buf)
{
    assert(!buf.first.empty() && ("Empty buffer name."));
    assert((buf.second != NULL) && ("Empty buffer."));

    this->buffers_list.insert(buf);
}

/*save inner schedules for computations*/
void function::save_computation_default_schedules(){

    for (auto const comp : this->get_computations())
    {
       comp->save_schedule_to_default() ;
    }

}

/*restore function & computation to without computations */
void function::restore_function_to_no_optimisations(){

    std::cout<<"undo **";
    for (auto const comp : this->get_computations())
    {
       comp->set_schedule_to_default() ;
       std::cout<<"undo comput";
    }
   
    this->vector_dimensions.clear();
    this->unroll_dimensions.clear() ;

    this->parallel_dimensions.clear();

    
}

/**
 * Construct a function with the name \p name.
 */
function::function(std::string name)
{
    assert(!name.empty() && ("Empty function name"));

    this->name = name;
    this->halide_stmt = Halide::Internal::Stmt();
    this->ast = NULL;
    this->context_set = NULL;
    this->use_low_level_scheduling_commands = false;
    this->_needs_rank_call = false;

    // Allocate an ISL context.  This ISL context will be used by
    // the ISL library calls within Tiramisu.
    this->ctx = isl_ctx_alloc();

    this->dep_read_after_write = NULL ;
    this->dep_write_after_write = NULL ;
    this->dep_write_after_read = NULL ;
    this->live_in_access = NULL ;
    this->live_out_access = NULL ;
};

isl_union_map *tiramisu::function::compute_dep_graph() {
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    isl_union_map *result = NULL;

    for (const auto &consumer : this->get_computations()) {
        DEBUG(3, tiramisu::str_dump("Computing the dependences involving the computation " +
                                    consumer->get_name() + "."));
        DEBUG(3, tiramisu::str_dump("Computing the accesses of the computation."));

        isl_union_map *accesses_union_map = NULL;
        std::vector < isl_map * > accesses_vector;
        generator::get_rhs_accesses(this, consumer, accesses_vector, false);

        DEBUG(3, tiramisu::str_dump("Vector of accesses computed."));

        if (!accesses_vector.empty()) {
            // Create a union map of the accesses to the producer.
            if (accesses_union_map == NULL) {
                isl_space *space = isl_map_get_space(accesses_vector[0]);
                assert(space != NULL);
                accesses_union_map = isl_union_map_empty(space);
            }

            for (size_t i = 0; i < accesses_vector.size(); ++i) {
                isl_map *reverse_access = isl_map_reverse(accesses_vector[i]);
                accesses_union_map = isl_union_map_union(isl_union_map_from_map(reverse_access),
                                                         accesses_union_map);
            }

            //accesses_union_map = isl_union_map_intersect_range(accesses_union_map, isl_union_set_from_set(isl_set_copy(consumer->get_iteration_domain())));
            //accesses_union_map = isl_union_map_intersect_domain(accesses_union_map, isl_union_set_from_set(isl_set_copy(consumer->get_iteration_domain())));

            DEBUG(3, tiramisu::str_dump("Accesses after filtering."));
            DEBUG(3, tiramisu::str_dump(isl_union_map_to_str(accesses_union_map)));

            if (result == NULL) {
                result = isl_union_map_copy(accesses_union_map);
                isl_union_map_free(accesses_union_map);
            } else {
                result = isl_union_map_union(result, accesses_union_map);
            }
        }
    }

    DEBUG(3, tiramisu::str_dump("Dep graph: "));
    if (result != NULL)
    {
        DEBUG(3, tiramisu::str_dump(isl_union_map_to_str(result)));
    }
    else
    {
        DEBUG(3, tiramisu::str_dump("Null."));
    }

    DEBUG_INDENT(-4);
    DEBUG(3, tiramisu::str_dump("End of function"));

    return result;
}

void tiramisu::function::calculate_dep_flow()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    assert(this->get_computations().size() >0 ) ;
    assert(this->get_computations()[0]->default_schedule!=NULL) ;

    DEBUG(3, tiramisu::str_dump(" generating depandencies graph"));

     isl_union_map * ref_graph = isl_union_map_reverse(this->compute_dep_graph()) ;

    DEBUG(3, tiramisu::str_dump(" the referencing union map is for dependecy analysis: "+std::string(isl_union_map_to_str(ref_graph))));

    int time_space_dim = isl_map_dim(this->get_computations()[0]->get_schedule(), isl_dim_out);

    std::string to_time_space_map_str = "[" ;

    std::string to_time_space_map_str_2 = "[" ;

    for(int i=0;i<time_space_dim;i++)
    {
        to_time_space_map_str+="t"+std::to_string(i) ;
        to_time_space_map_str_2+="t"+std::to_string(i) ;

        if(i != (time_space_dim -1 ))
        {
            to_time_space_map_str+="," ;
            to_time_space_map_str_2+="," ;
            
        }

    }
    std::string ready_time_str = to_time_space_map_str+"]->" + to_time_space_map_str_2+"]" ;// without {} yet

    DEBUG(3, tiramisu::str_dump("using to generate time stamp tmp map "+ready_time_str));


    std::string access_start = "{}" ;

    // S0[i,j] -> buff[i] the writing stmt
    isl_union_map * write_access = isl_union_map_read_from_str(this->get_isl_ctx(),access_start.c_str()) ;

    isl_union_map * isl_schedule = isl_union_map_read_from_str(this->get_isl_ctx(),access_start.c_str()) ;


    std::string identity = "" ;

    isl_map * isl_identity = NULL ;
    
    for(auto& comput : this->get_computations())
    {
        identity = "{"+comput->get_name() +ready_time_str + "}" ;

        isl_identity = isl_map_read_from_str(this->get_isl_ctx(),identity.c_str()) ;

        // TODO : use default schedule instead
        isl_map * corrected = isl_map_apply_range(isl_map_copy(comput->default_schedule),isl_identity) ;

        DEBUG(10, tiramisu::str_dump(" - > compuatation's schedule to time stamp op result is : "+std::string(isl_map_to_str(corrected))));

        isl_schedule = isl_union_map_union(isl_schedule , isl_union_map_from_map(corrected)) ;

        write_access = isl_union_map_union(write_access,isl_union_map_from_map(isl_map_copy(comput->get_access_relation()))) ;
        
    } 

    write_access = isl_union_map_intersect_domain(write_access, this->get_iteration_domain());

    DEBUG(3, tiramisu::str_dump("the overall function schedule is : "+std::string(isl_union_map_to_str(isl_schedule))));

    DEBUG(3, tiramisu::str_dump("the write access for computations is : "+std::string(isl_union_map_to_str(write_access))));
    
    isl_union_map * read_access = isl_union_map_apply_range(
        isl_union_map_copy(ref_graph),
        isl_union_map_copy(write_access)
    );
    //combine reads previous with thier access to establish the read access S0[i,j] -> buf2[j] in read 

    DEBUG(3, tiramisu::str_dump(" The read access for computations : "+std::string(isl_union_map_to_str(read_access))));

    isl_union_access_info *info = isl_union_access_info_from_sink( isl_union_map_copy(read_access)) ;

    info = isl_union_access_info_set_schedule_map(info,isl_union_map_copy(isl_schedule)) ;

    info = isl_union_access_info_set_must_source(info,isl_union_map_copy(write_access)) ;

    isl_union_flow * flow = isl_union_access_info_compute_flow(info) ;

    //DEBUG(3, tiramisu::str_dump(" dependency analysis with must for read after write ( no predicats ) result  : "+std::string(isl_union_flow_to_str(flow))));

    isl_union_map * read_after_write_dep = isl_union_flow_get_full_must_dependence(flow) ;

    isl_union_map * read_from_outside = isl_union_flow_get_must_no_source(flow) ;

    DEBUG(3, tiramisu::str_dump(" read after write True dependencies are in the form { last_write_access -> the read statement } : "+std::string(isl_union_map_to_str(read_after_write_dep))));
       
    DEBUG(3, tiramisu::str_dump(" live-in : the computations / statments with these read access have not been written in this function (outside value)  : "+std::string(isl_union_map_to_str(read_from_outside))));
    

    info = isl_union_access_info_from_sink(isl_union_map_copy(write_access)) ;

    info = isl_union_access_info_set_schedule_map(info,isl_union_map_copy(isl_schedule)) ;

    info = isl_union_access_info_set_must_source(info,isl_union_map_copy(write_access)) ;

    flow = isl_union_access_info_compute_flow(info) ;

    isl_union_map * write_after_write_dep = isl_union_flow_get_full_must_dependence(flow) ;

    DEBUG(3, tiramisu::str_dump(" write after write dependencies are { last_previous_write -> new write stmt } : "+std::string(isl_union_map_to_str(write_after_write_dep))));


    isl_union_map * not_last_writes = isl_union_map_range_factor_range( isl_union_map_copy(write_after_write_dep)) ;
    
    isl_union_map * live_out = isl_union_map_subtract(
        isl_union_map_copy(write_access),
        isl_union_map_copy(not_last_writes)
    );

    live_out = isl_union_map_intersect_domain(live_out, this->get_iteration_domain());

    DEBUG(3, tiramisu::str_dump(" live out last access are : "+std::string(isl_union_map_to_str(live_out))));

    isl_union_map * read_without_write_stmt = isl_union_map_subtract(isl_union_map_copy(read_access), isl_union_map_copy(write_access)) ;

    info = isl_union_access_info_from_sink(isl_union_map_copy(write_access)) ;

    info = isl_union_access_info_set_schedule_map(info,isl_union_map_copy(isl_schedule)) ;

    info = isl_union_access_info_set_may_source(info,isl_union_map_copy(read_without_write_stmt)) ;

    info = isl_union_access_info_set_kill(info,isl_union_map_copy(write_access)) ;

    flow = isl_union_access_info_compute_flow(info) ;

   

    //DEBUG(3, tiramisu::str_dump(" dependency analysis for WAR dep : "+std::string(isl_union_flow_to_str(flow))));
    
    //isl_union_map * initialisation_access = isl_union_flow_get_must_no_source(flow) ;

    isl_union_map * anti_dependencies = isl_union_flow_get_full_may_dependence(flow) ;

    DEBUG(3, tiramisu::str_dump(" write after read anti_dependencies are in the form { last_previous_read -> new write stmt } : "+std::string(isl_union_map_to_str(anti_dependencies))));

    //DEBUG(3, tiramisu::str_dump(" the initialisation stmt writes with no previous read before are : "+std::string(isl_union_map_to_str(initialisation_access))));
      
    this->dep_read_after_write = read_after_write_dep ;

    this->dep_write_after_read = anti_dependencies ;

    this->dep_write_after_write = write_after_write_dep ;

    this->live_in_access = read_from_outside ;

    this->live_out_access = live_out ;

  
    
    DEBUG_INDENT(-4);

}

 std::vector<std::pair<computation *,isl_set *>> tiramisu::function::get_live_out_computations_from_buffers_deps() const  
{
    

    assert(this->live_out_access!=NULL) ;

    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    DEBUG(3, tiramisu::str_dump(" identifing the computations with the live-out access i.e: last to write to a buffer "));


    std::vector<std::pair<tiramisu::computation *,isl_set *>> live_computations ;

    isl_union_map * live_out = isl_union_map_copy(this->live_out_access) ;

    for(auto& computation: this->get_computations())
    {   
        isl_union_map * intersect = isl_union_map_intersect(
            isl_union_map_from_map(isl_map_copy(computation->get_access_relation())),
            isl_union_map_copy(live_out)
        );

        if( ! isl_union_map_is_empty(intersect) )
        {
            isl_set * res = isl_map_domain(isl_map_from_union_map(intersect)) ;
            // compuation figures in live-out so push
            live_computations.push_back( std::make_pair(computation,res)) ;

            DEBUG(3, tiramisu::str_dump(" the computation "+computation->get_name()+" figures in the live_out access "));

        }

    }

    DEBUG_INDENT(-4);

    return live_computations ;
}

 std::vector<std::pair<computation *,isl_set *>> tiramisu::function::get_live_in_computations_from_buffers_deps() const  
{
    

    assert(this->live_in_access!=NULL) ;

    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    DEBUG(3, tiramisu::str_dump(" identifing the computations with the live-in access i.e: last to write to a buffer "));


    std::vector<std::pair<tiramisu::computation *,isl_set *>> live_computations ;

    isl_union_map * live_in = isl_union_map_copy(this->live_in_access) ;

    for(auto& computation: this->get_computations())
    {   
        isl_union_map * intersect = isl_union_map_intersect(
            isl_union_map_from_map(isl_map_copy(computation->get_access_relation())),
            isl_union_map_copy(live_in)
        );

        if( ! isl_union_map_is_empty(intersect) )
        {
            isl_set * res = isl_map_domain(isl_map_from_union_map(intersect)) ;
        
            live_computations.push_back( std::make_pair(computation,res)) ;

            DEBUG(3, tiramisu::str_dump(" the computation "+computation->get_name()+" figures in the live_in access "));

        }

    }

    DEBUG_INDENT(-4);

    return live_computations ;
}

bool tiramisu::function::loop_parallelization_is_legal(tiramisu::var par_dim_var, std::vector<tiramisu::computation *> fuzed_computations )
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    assert(par_dim_var.get_name().length() > 0);
    assert(!this->get_name().empty());

    assert(this->dep_read_after_write != NULL ) ;
    assert(this->dep_write_after_write != NULL ) ;
    assert(this->dep_write_after_read != NULL ) ;
    assert(fuzed_computations.size()>0) ;

    computation * first_computation = fuzed_computations[0]  ;
    
    DEBUG(3, tiramisu::str_dump(" var parallelization check is : "+par_dim_var.get_name()));

    
     std::vector<std::string> original_loop_level_names = first_computation->get_loop_level_names();

    std::vector<int> dimensions =
        first_computation->get_loop_level_numbers_from_dimension_names({par_dim_var.get_name()});

    first_computation->check_dimensions_validity(dimensions);

    bool result = this->loop_parallelization_is_legal(dimensions[0],fuzed_computations) ;

    DEBUG_INDENT(-4);

    return result ;
}


bool tiramisu::function::loop_parallelization_is_legal(int dim_parallel , std::vector<tiramisu::computation *> fuzed_computations)
{

    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    assert(!this->get_name().empty());

    assert(this->dep_read_after_write != NULL ) ;
    assert(this->dep_write_after_write != NULL ) ;
    assert(this->dep_write_after_read != NULL ) ;
    assert(fuzed_computations.size()>0) ;

    computation * first_computation = fuzed_computations[0]  ;
    
    std::vector<std::string> original_loop_level_names = first_computation->get_loop_level_names();

    int par_dim = tiramisu::loop_level_into_dynamic_dimension(dim_parallel);


    DEBUG(3, tiramisu::str_dump(" par dim number is : "+std::to_string(par_dim)));



    // extracting deps

     isl_union_map * read_after_write_dep = isl_union_map_range_factor_domain(
        isl_union_map_copy(this->dep_read_after_write)) ;

    isl_union_map * write_after_read_dep = isl_union_map_range_factor_domain(
        isl_union_map_copy(this->dep_write_after_read)) ;

    isl_union_map * write_after_write_dep = isl_union_map_range_factor_domain(
        isl_union_map_copy(this->dep_write_after_write)) ;



    isl_union_map * all_deps = isl_union_map_union(
        read_after_write_dep,
        write_after_read_dep
        ) ;

    // all the deps in 1 union map
    all_deps = isl_union_map_union(all_deps,write_after_write_dep) ;


    DEBUG(3, tiramisu::str_dump(" all the dependencies are  : "+std::string(isl_union_map_to_str(all_deps))));

    // all current schedules in 1 union map
    std::string empty_union = "{}" ;
    std::string empty_time  = "" ;

    isl_union_map * schedules = isl_union_map_read_from_str(this->get_isl_ctx(),empty_union.c_str()) ;

    isl_map * sche = NULL ;

    for( auto& computation: fuzed_computations)
    {
        sche = isl_map_copy(computation->get_schedule()) ;

        sche = isl_map_set_tuple_name(sche,isl_dim_out,empty_time.c_str()) ;

        schedules = isl_union_map_union(schedules,isl_union_map_from_map(sche)) ;

    }
    

    DEBUG(3, tiramisu::str_dump(" all the used schedules are  : "+std::string(isl_union_map_to_str(schedules))));

    // application to discard unused dep & modelize them in thier time space

    all_deps = isl_union_map_apply_range(all_deps,isl_union_map_copy( schedules )) ;

    all_deps = isl_union_map_apply_domain(all_deps,isl_union_map_copy( schedules )) ;

    DEBUG(3, tiramisu::str_dump(" all the used dependencies union map are  : "+std::string(isl_union_map_to_str(all_deps))));

    isl_map * equation_map = isl_map_from_union_map(all_deps) ;

    DEBUG(3, tiramisu::str_dump(" all the used dependencies after transformed to map are  : "+std::string(isl_map_to_str(equation_map))));

    bool overall_legality = false ;

    /*
        equate adds restriction that both elements are equal
        we suppose that legality is checked elsewhere ; so we need to check for loop caried dependencies only
        if adding equation of == between input set & output set of map for a dimention strictly before the parallel one is empty means : 
            dep is not a carried one for the parallel loop lvl

        else 
            if all previous equations added does not make the map empty then the last possiblity is:
                dep is within the same loop iteration; then parallel is true ( true if equate doesnt make the map empty)
                else it's false
                
    */
    for(int i=0;i<par_dim;i++)
    {
        equation_map = isl_map_equate(equation_map,isl_dim_in,i,isl_dim_out,i) ;

        if(isl_map_is_empty(equation_map))
        {
            overall_legality = true ;
            DEBUG(10, tiramisu::str_dump(" parallalization is legal "));
            break ;
        }
    
    }

    if(!overall_legality)
    {
        equation_map = isl_map_equate(equation_map,isl_dim_in,par_dim,isl_dim_out,par_dim) ;

        if(isl_map_is_empty(equation_map))
        {
            overall_legality = false ;
            DEBUG(3, tiramisu::str_dump(" parallalization is illegal "));
        }
        else{
            overall_legality = true ;
            DEBUG(3, tiramisu::str_dump(" parallalization is legal "));
        }
    }


    DEBUG_INDENT(-4); 
    return overall_legality ;


}

const std::map<std::string, tiramisu::buffer *> tiramisu::function::get_mapping() const
{
  return this->mapping;
}

void  tiramisu::function::add_mapping(std::pair<std::string,tiramisu::buffer *> p)
{
  this->mapping.insert(p);
}

/**
 * This function takes computation pts to C1 and C2 which are the first and the last computation
 * of the fucntion.
 * Automatic_communication fonction verifies if the user wants to manage the copies manually or not.
 * By default, the data management will be done automatically, and the copies will be inserted
 * in the begining and at the end of the gpu funcion.
 * The inputs of the function will be copied to the device before the computations, and the
 * outputs of the functions will be copied back to the host at the end.
 * We have two cases copies to constant memory and the default case which is copies to the global memory.
 */
void function::Automatic_communication(tiramisu::computation* c1, tiramisu::computation* c2)
{
    assert(c1 != nullptr && "C1 = NULL ");
    assert(c2 != nullptr && "C2 = NULL ");
    assert(c1->get_predecessor() == nullptr && "C1 must be the computation that hasn't a predessessor ");
    assert(c2->get_successor() == nullptr && "C2 must be the computation that hasn't a successor ");
    std::map<std::string, tiramisu::buffer*> buff = this->get_buffers();
    std::map<std::string, tiramisu::buffer*> mp = this->mapping;
    std::map<std::string, tiramisu::buffer*>::iterator it ;
    std::string name, cpt_name;
    int i = 1;
    tiramisu::computation* first_cpt = c1;
    tiramisu::computation* last_cpt = c2;
    for (it = mp.begin(); it != mp.end(); ++it)
    {
        name = it->first;
        assert(it->second->get_argument_type() == tiramisu::a_temporary  && "Mapping field should contain a string corresponding to the name of a cpu buffer and a ptr to the corresponding gpu buffer ");
        if (it->second->automatic_gpu_copy == true)
        {
            cpt_name= "cpt" + std::to_string(i);
            i++;
            switch (it->second->location)
            {
            case cuda_ast::memory_location::constant:
                if (buff.find(name)->second->get_argument_type() == tiramisu::a_input)
                {
                    tiramisu::computation* c =  new tiramisu::computation(cpt_name, {},
                        memcpy(*(buff.find(name)->second),(*(it->second))));
                    (*c).then((*first_cpt), computation::root);
                    first_cpt = c;
                }
                break;
            default:
                if (buff.find(name)->second->get_argument_type() == tiramisu::a_input)
                {
                    tiramisu::computation* c =  new tiramisu::computation(cpt_name, {},
                        memcpy(*(buff.find(name)->second),(*(it->second))));
                    (*c).then((*first_cpt), computation::root);
                    first_cpt = c;
                }
                if (buff.find(name)->second->get_argument_type() == tiramisu::a_output)
                {
                    tiramisu::computation* c =  new tiramisu::computation(cpt_name, {},
                        memcpy((*(it->second)),*(buff.find(name)->second)));
                    (*last_cpt).then((*c), computation::root);
                    last_cpt = c;
                }
                break;
            }
        }
        else
        {
            DEBUG(3, tiramisu::str_dump("Communication should be done manually !"));
        }
    }
}

// TODO: get_live_in_computations() does not consider the case of "maybe"
// live-out (non-affine control flow, ...).
std::vector<tiramisu::computation *> tiramisu::function::get_live_in_computations()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    assert((this->get_computations().size() > 0) &&
           "The function should have at least one computation.");

    std::vector < tiramisu::computation * > first;
    isl_union_map *deps = this->compute_dep_graph();

    if (deps != NULL) {
        if (isl_union_map_is_empty(deps) == isl_bool_false) {
            // The domains and the ranges of the dependences
            isl_union_set *domains = isl_union_map_domain(isl_union_map_copy(deps));
            isl_union_set *ranges = isl_union_map_range(isl_union_map_copy(deps));

            DEBUG(3, tiramisu::str_dump("Ranges of the dependence graph.", isl_union_set_to_str(ranges)));
            DEBUG(3, tiramisu::str_dump("Domains of the dependence graph.", isl_union_set_to_str(domains)));

            /** In a dependence graph, since dependences create a chain (i.e., the end of
             *  a dependence is the beginning of the following), then each range of
             *  a dependence has a set domains that correspond to it (i.e., that their
             *  union is equal to it).  If a domain exists but does not have ranges that
             *  are equal to it, then that domain is the first domain.
             *
             *  To compute those domains that do not have corresponding ranges, we
             *  compute (domains - ranges).
             *
             *  These domains that do not have a corresponding range (i.e., are not
             *  produced by previous computations) and that are not defined (i.e., do
             *  not have any expression) are live-in.
             */
            isl_union_set *first_domains = isl_union_set_subtract(domains, ranges);
            DEBUG(3, tiramisu::str_dump("Domains - Ranges :", isl_union_set_to_str(first_domains)));

            if (isl_union_set_is_empty(first_domains) == isl_bool_false) {
                for (const auto &c : this->body) {
                    isl_space *sp = isl_set_get_space(c->get_iteration_domain());
                    isl_set *s = isl_set_universe(sp);
                    isl_union_set *intersect =
                            isl_union_set_intersect(isl_union_set_from_set(s),
                                                    isl_union_set_copy(first_domains));

                    if ((isl_union_set_is_empty(intersect) == isl_bool_false) &&
                        (c->get_expr().is_defined() == false))
                    {
                        first.push_back(c);
                    }
                    isl_union_set_free(intersect);
                }

                DEBUG(3, tiramisu::str_dump("First computations:"));
                for (const auto &c : first) {
                    DEBUG(3, tiramisu::str_dump(c->get_name() + " "));
                }
            } else {
                // If the difference between domains and ranges is empty, then
                // all the computations of the program are recursive (assuming
                // that the set of dependences is not empty).
                first = this->body;
            }

            isl_union_set_free(first_domains);
        } else {
            // If the program does not have any dependence, then
            // all the computations should be considered as the first
            // computations.
            first = this->body;
        }

        isl_union_map_free(deps);
    }

    DEBUG(3, tiramisu::str_dump("Removing inline computations."));
    std::vector<computation* > result;
    for (computation * c: first)
        if (! c->is_inline_computation())
            result.push_back(c);

    DEBUG_INDENT(-4);

    return result;
}

// TODO: get_live_out_computations() does not consider the case of "maybe"
// live-out (non-affine control flow, ...).
std::vector<tiramisu::computation *> tiramisu::function::get_live_out_computations()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    assert((this->get_computations().size() > 0) &&
           "The function should have at least one computation.");

    std::vector<tiramisu::computation *> first;
    isl_union_map *deps = this->compute_dep_graph();

    if (deps != NULL)
    {
        if (isl_union_map_is_empty(deps) == isl_bool_false)
        {
            // The domains and the ranges of the dependences
            isl_union_set *domains = isl_union_map_domain(isl_union_map_copy(deps));
            isl_union_set *ranges = isl_union_map_range(isl_union_map_copy(deps));

            DEBUG(3, tiramisu::str_dump("Ranges of the dependence graph.", isl_union_set_to_str(ranges)));
            DEBUG(3, tiramisu::str_dump("Domains of the dependence graph.", isl_union_set_to_str(domains)));

            /** In a dependence graph, since dependences create a chain (i.e., the end of
             *  a dependence is the beginning of the following), then each range of
             *  a dependence has a set domains that correspond to it (i.e., that their
             *  union is equal to it).  If a range exists but does not have domains that
             *  are equal to it, then that range is the last range.
             *
             *  To compute those ranges that do not have corresponding domains, we
             *  compute (ranges - domains).
             */
            isl_union_set *last_ranges = isl_union_set_subtract(ranges, domains);
            DEBUG(3, tiramisu::str_dump("Ranges - Domains :", isl_union_set_to_str(last_ranges)));

            if (isl_union_set_is_empty(last_ranges) == isl_bool_false)
            {
                for (const auto &c : this->body)
                {
                    isl_space *sp = isl_set_get_space(c->get_iteration_domain());
                    isl_set *s = isl_set_universe(sp);
                    isl_union_set *intersect =
                        isl_union_set_intersect(isl_union_set_from_set(s),
                                                isl_union_set_copy(last_ranges));

                    if (isl_union_set_is_empty(intersect) == isl_bool_false)
                    {
                        first.push_back(c);
                    }
                    isl_union_set_free(intersect);
                }

                DEBUG(3, tiramisu::str_dump("Last computations:"));
                for (const auto &c : first)
                {
                    DEBUG(3, tiramisu::str_dump(c->get_name() + " "));
                }
            }
            else
            {
                // If the difference between ranges and domains is empty, then
                // all the computations of the program are recursive (assuming
                // that the set of dependences is not empty).
                first = this->body;
            }

            isl_union_set_free(last_ranges);
        }
        else
        {
            // If the program does not have any dependence, then
            // all the computations should be considered as the last
            // computations.
            first = this->body;
        }
        isl_union_map_free(deps);
    }
    else
    {
        // If the program does not have any dependence, then
        // all the computations should be considered as the last
        // computations.
        first = this->body;
    }

    DEBUG(3, tiramisu::str_dump("Removing inline computations."));
    std::vector<computation* > result;
    for (computation * c: first)
        if (! c->is_inline_computation())
            result.push_back(c);

    assert((result.size() > 0) && "The function should have at least one last computation.");

    DEBUG_INDENT(-4);


    return result;
}

void tiramisu::function::compute_bounds()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    isl_union_map *Dep = this->compute_dep_graph();
    DEBUG(3, tiramisu::str_dump("Dependences computed."));
    isl_union_map *Reverse = isl_union_map_reverse(isl_union_map_copy(Dep));
    DEBUG(3, tiramisu::str_dump("Reverse of dependences:", isl_union_map_to_str(Reverse)));
    // Compute the vector of the last computations in the dependence graph
    // (i.e., the computations that do not have any consumer).
    std::vector<tiramisu::computation *> first = this->get_live_out_computations();

    assert(first.size() > 0);

    isl_union_set *Domains = NULL;
    Domains = isl_union_set_empty(isl_set_get_space(first[0]->get_iteration_domain()));
    for (auto c : first)
    {
        Domains = isl_union_set_union(Domains,
                                      isl_union_set_from_set(isl_set_copy(c->get_iteration_domain())));
    }
    DEBUG(3, tiramisu::str_dump("The domains of the last computations are:",
                                isl_union_set_to_str(Domains)));

    // Compute "Producers", the union of the iteration domains of the computations
    // that computed "last".
    isl_union_set *Producers = isl_union_set_apply(isl_union_set_copy(Domains),
                               isl_union_map_copy(Reverse));
    DEBUG(3, tiramisu::str_dump("The producers of the last computations are:",
                                isl_union_set_to_str(Producers)));

    // If the graph of dependences has recursive dependences, then the intersection of
    // the old producers and the new producers will not be empty (i.e., the old producer and the new producer
    // are the same).
    // In this case, we should subtract the common domain so that in the next iterations of the
    // the algorithm we do not get the same computation again and again (since we have a recursive dependence).
    // This is equivalent to removing the recursive dependence (we remove its image instead of removing it).
    isl_union_set *old_Producers = isl_union_set_copy(Domains);
    isl_union_set *intersection = isl_union_set_intersect(old_Producers, isl_union_set_copy(Producers));
    if (isl_union_set_is_empty(intersection) == isl_bool_false)
    {
        isl_union_set *common_computations = isl_union_set_universe(intersection);
        Producers = isl_union_set_subtract(Producers, common_computations);
        DEBUG(3, tiramisu::str_dump("After eliminating the effect of recursive dependences.",
                                    isl_union_set_to_str(Producers)));
    }


    // Propagation of bounds
    DEBUG(3, tiramisu::str_dump("Propagating the bounds over all computations."));
    DEBUG_INDENT(4);
    while (isl_union_set_is_empty(Producers) == isl_bool_false)
    {
        for (auto c : this->get_computations())
        {
            DEBUG(3, tiramisu::str_dump("Computing the domain (bounds) of the computation: " + c->get_name()));
            isl_union_set *c_dom = isl_union_set_from_set(isl_set_copy(c->get_iteration_domain()));
            DEBUG(3, tiramisu::str_dump("Domain of the computation: ", isl_union_set_to_str(c_dom)));
            isl_union_set *prods = isl_union_set_copy(Producers);
            DEBUG(3, tiramisu::str_dump("Producers : ", isl_union_set_to_str(prods)));
            // Filter the producers to remove the domains of all the computations except the domain of s1
            // Keep only the computations that have the same space as s1.
            isl_union_set *filter = isl_union_set_universe(isl_union_set_copy(c_dom));
            isl_union_set *c_prods = isl_union_set_intersect(isl_union_set_copy(filter), prods);
            DEBUG(3, tiramisu::str_dump("After keeping only the producers that have the same space as the domain.",
                                        isl_union_set_to_str(c_prods)));

            // If this is not an update operation, we can update its domain, otherwise
            // we do not update the domain and keep the one provided by the user.
            if (c->has_multiple_definitions() == false)
            {
                // REC TODO: in the documentation of compute_bounds indicate that compute_bounds does not update the bounds of update operations
                if ((isl_union_set_is_empty(c_prods) == isl_bool_false))
                {
                    if ((isl_set_plain_is_universe(c->get_iteration_domain()) == isl_bool_true))
                    {
                        DEBUG(3, tiramisu::str_dump("The iteration domain of the computation is a universe."));
                        DEBUG(3, tiramisu::str_dump("The new domain of the computation = ",
                                                    isl_union_set_to_str(c_prods)));
                        c->set_iteration_domain(isl_set_from_union_set(isl_union_set_copy(c_prods)));
                    }
                    else
                    {
                        DEBUG(3, tiramisu::str_dump("The iteration domain of the computation is NOT a universe."));
                        isl_union_set *u = isl_union_set_union(isl_union_set_copy(c_prods),
                                                               isl_union_set_copy(c_dom));
                        c->set_iteration_domain(isl_set_from_union_set(isl_union_set_copy(u)));
                        DEBUG(3, tiramisu::str_dump("The new domain of the computation = ",
                                                    isl_union_set_to_str(u)));
                    }
                }
            }
            else
            {
                assert((isl_set_plain_is_universe(c->get_iteration_domain()) == isl_bool_false) &&
                       "The iteration domain of an update should not be universe.");
                assert((isl_set_is_empty(c->get_iteration_domain()) == isl_bool_false) &&
                       "The iteration domain of an update should not be empty.");
            }

            DEBUG(3, tiramisu::str_dump(""));
        }

        old_Producers = isl_union_set_copy(Producers);
        Producers = isl_union_set_apply(isl_union_set_copy(Producers), isl_union_map_copy(Reverse));
        DEBUG(3, tiramisu::str_dump("The new Producers : ", isl_union_set_to_str(Producers)));

        // If the graph of dependences has recursive dependences, then the intersection of
        // the old producers and the new producers will not be empty (i.e., the old producer and the new producer
        // are the same).
        // In this case, we should subtract the common domain so that in the next iterations of the
        // the algorithm we do not get the same computation again and again (since we have a recursive dependence).
        // This is equivalent to removing the recursive dependence (we remove its image instead of removing it).
        intersection = isl_union_set_intersect(old_Producers, isl_union_set_copy(Producers));
        if (isl_union_set_is_empty(intersection) == isl_bool_false)
        {
            isl_union_set *common_computations = isl_union_set_universe(intersection);
            Producers = isl_union_set_subtract(Producers, common_computations);
            DEBUG(3, tiramisu::str_dump("After eliminating the effect of recursive dependences.",
                                        isl_union_set_to_str(Producers)));
        }

    }
    DEBUG_INDENT(-4);

    DEBUG(3, tiramisu::str_dump("After propagating bounds:"));
    for (auto c : this->get_computations())
    {
        DEBUG(3, tiramisu::str_dump(isl_set_to_str(c->get_iteration_domain())));
    }

    DEBUG_INDENT(-4);
    DEBUG(3, tiramisu::str_dump("End of function"));
}

void tiramisu::function::dump_dep_graph()
{

    tiramisu::str_dump("Dependence graph:\n");
    isl_union_map *deps = isl_union_map_copy(this->compute_dep_graph());
    isl_union_map_dump(deps);
}

void function::dump_halide_stmt() const
{
    tiramisu::str_dump("\n\n");
    tiramisu::str_dump("\nGenerated Halide Low Level IR:\n");
    std::cout << this->get_halide_stmt();
    tiramisu::str_dump("\n\n\n\n");
}

void function::dump_trimmed_time_processor_domain() const
{
    // Create time space domain

    if (ENABLE_DEBUG)
    {
        tiramisu::str_dump("\n\nTrimmed Time-processor domain:\n");

        tiramisu::str_dump("Function " + this->get_name() + ":\n");
        for (const auto &comp : this->get_computations())
        {
            isl_set_dump(comp->get_trimmed_time_processor_domain());
        }

        tiramisu::str_dump("\n\n");
    }
}

void function::dump_time_processor_domain() const
{
    // Create time space domain

    if (ENABLE_DEBUG)
    {
        tiramisu::str_dump("\n\nTime-processor domain:\n");

        tiramisu::str_dump("Function " + this->get_name() + ":\n");
        for (const auto &comp : this->get_computations())
        {
            isl_set_dump(comp->get_time_processor_domain());
        }

        tiramisu::str_dump("\n\n");
    }
}

void function::dump_sched_graph_dfs(computation * comp,
                                    std::unordered_set<computation *> &visited)
{
    // Do not visit anything that was already returned
    if (visited.find(comp) != visited.end())
        return;

    visited.insert(comp);

    for (auto &edge: this->sched_graph[comp])
    {
        const std::string level = ((edge.second == computation::root_dimension) ?
                                   "root" :
                                   std::to_string(edge.second));

        DEBUG(3, tiramisu::str_dump(comp->get_unique_name() +
                                    "=[" + level + "]=>" +
                                    edge.first->get_unique_name()));

        dump_sched_graph_dfs(edge.first, visited);
    }
}

void function::dump_sched_graph()
{
    DEBUG(3, tiramisu::str_dump("Number of schedule graph roots is " +
                                std::to_string(this->starting_computations.size())));
    DEBUG(3, tiramisu::str_dump("The roots are:"));

    for (auto root: this->starting_computations)
        DEBUG(3, tiramisu::str_dump(" * " + root->get_unique_name()));

    // Contains all nodes that have been visited
    std::unordered_set<computation *> visited;

    DEBUG(3, tiramisu::str_dump("Displaying schedule graph"));

    for (auto &comp: this->starting_computations)
    {
        dump_sched_graph_dfs(comp, visited);
    }

    DEBUG(3, tiramisu::str_dump("Finished displaying schedule graph"));
}

bool function::is_sched_graph_tree_dfs(computation * comp,
                                       std::unordered_set<computation *> &visited)
{
    // Do not visit anything that was already returned
    if (visited.find(comp) != visited.end())
        return false;

    visited.insert(comp);

    for (auto &edge: this->sched_graph[comp])
    {
        if (!is_sched_graph_tree_dfs(edge.first, visited))
            return false;
    }

    return true;
}

bool function::is_sched_graph_tree()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    if (this->starting_computations.size() != 1)
    {
        DEBUG_INDENT(-4);
        return false;
    }

    // Contains all nodes that have been visited
    std::unordered_set<computation *> visited;

    for (auto &comp: this->starting_computations)
    {
        if (!is_sched_graph_tree_dfs(comp, visited))
        {
            DEBUG_INDENT(-4);
            return false;
        }
    }

    DEBUG_INDENT(-4);
    return true;
}


/**
  * Get the arguments of the function.
  */
// @{
const std::vector<tiramisu::buffer *> &function::get_arguments() const {
    return function_arguments;
}
// @}

/**
  * Return a map that represents the buffers of the function.
  * The buffers of the function are buffers that are either passed
  * to the function as arguments or are buffers that are declared
  * and allocated within the function itself.
  * The names of the buffers are used as a key for the map.
  */
// @{
const std::map<std::string, tiramisu::buffer *> &function::get_buffers() const
{
    return buffers_list;
}
// @}

/**
   * Return a vector of the computations of the function.
   * The order of the computations in the vector does not have any
   * effect on the actual order of execution of the computations.
   * The order of execution of computations is specified through the
   * schedule.
   */
// @{
const std::vector<computation *> &function::get_computations() const
{
    return body;
}
// @}

/**
  * Return the context of the function. i.e. an ISL set that
  * represents constraints over the parameters of the functions
  * (a parameter is an invariant of the function).
  * An example of a context set is the following:
  *          "[N,M]->{: M>0 and N>0}"
  * This context set indicates that the two parameters N and M
  * are strictly positive.
  */
isl_set *function::get_program_context() const
{
    if (context_set != NULL)
    {
        return isl_set_copy(context_set);
    }
    else
    {
        return NULL;
    }
}

/**
  * Get the name of the function.
  */
const std::string &function::get_name() const
{
    return name;
}

/**
  * Return a vector representing the invariants of the function
  * (symbolic constants or variables that are invariant to the
  * function i.e. do not change their value during the execution
  * of the function).
  */
// @{
const std::vector<tiramisu::constant> &function::get_invariants() const
{
    return invariants;
}
// @}

/**
  * Return the Halide statement that represents the whole
  * function.
  * The Halide statement is generated by the code generator.
  * This function should not be called before calling the code
  * generator.
  */
Halide::Internal::Stmt function::get_halide_stmt() const
{
    assert(halide_stmt.defined() && ("Empty Halide statement"));

    return halide_stmt;
}

/**
  * Return the isl context associated with this function.
  */
isl_ctx *function::get_isl_ctx() const
{
    return ctx;
}

/**
  * Return the isl ast associated with this function.
  */
isl_ast_node *function::get_isl_ast() const
{
    assert((ast != NULL) && ("You should generate an ISL ast first (gen_isl_ast())."));

    return ast;
}

/**
  * Get the iterator names of the function.
  */
const std::vector<std::string> &function::get_iterator_names() const
{
    return iterator_names;
}

/**
  * Return true if the computation \p comp should be parallelized
  * at the loop level \p lev.
  */
bool function::should_parallelize(const std::string &comp, int lev) const
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    assert(!comp.empty());
    assert(lev >= 0);

    bool found = false;

    DEBUG(10, tiramisu::str_dump("Checking if the computation " + comp +
                                 " should be parallelized" +
                                 " at the loop level " + std::to_string(lev)));

    for (const auto &pd : this->parallel_dimensions)
    {
        DEBUG(10, tiramisu::str_dump("Checking if the computation " + comp +
                                     " at the loop level " + std::to_string(lev) +
                                     " is equal to the tagged computation " +
                                     pd.first + " at the level " + std::to_string(pd.second)));

        if ((pd.first == comp) && (pd.second == lev))
        {
            found = true;

            DEBUG(10, tiramisu::str_dump("Yes equal."));
        }
    }

    std::string str = "Dimension " + std::to_string(lev) +
                      (found ? " should" : " should not")
                       + " be mapped to CPU thread.";
    DEBUG(10, tiramisu::str_dump(str));

    DEBUG_INDENT(-4);

    return found;
}

/**
* Return the unrolling factor used to unroll the computation \p comp
* at the loop level \p lev.
*/
int function::get_unrolling_factor(const std::string &comp, int lev) const
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    assert(!comp.empty());
    assert(lev >= 0);

    int unrolling_factor = -1;
    bool found = false;

    for (const auto &pd : this->unroll_dimensions)
    {
        if ((std::get<0>(pd) == comp) && (std::get<1>(pd) == lev))
        {
            unrolling_factor = std::get<2>(pd);
            found = true;
        }
    }

    std::string str = "Dimension " + std::to_string(lev) +
                      (found ? " should" : " should not")
                       + " be unrolled with a factor of " +
                       std::to_string(unrolling_factor);
    DEBUG(10, tiramisu::str_dump(str));

    DEBUG_INDENT(-4);

    return unrolling_factor;
}

/**
* Return the vector length of the computation \p comp at
* at the loop level \p lev.
*/
int function::get_vector_length(const std::string &comp, int lev) const
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    assert(!comp.empty());
    assert(lev >= 0);

    int vector_length = -1;
    bool found = false;

    for (const auto &pd : this->vector_dimensions)
    {
        if ((std::get<0>(pd) == comp) && (std::get<1>(pd) == lev))
        {
            vector_length = std::get<2>(pd);
            found = true;
        }
    }

    std::string str = "Dimension " + std::to_string(lev) +
                      (found ? " should" : " should not")
                       + " be vectorized with a vector length of " +
                       std::to_string(vector_length);
    DEBUG(10, tiramisu::str_dump(str));

    DEBUG_INDENT(-4);

    return vector_length;
}

computation * function::get_first_cpt() {
    if (this->is_sched_graph_tree()) {
        tiramisu::computation* cpt = this->sched_graph.begin()->first;
        while (cpt->get_predecessor() != NULL) {
            cpt = cpt->get_predecessor();
        }
        return cpt;
    } else {
        DEBUG(3, tiramisu::str_dump(" this->is_sched_graph_tree(): false."));
        return NULL;
    }
}

computation * function::get_last_cpt() {
    if (this->is_sched_graph_tree()) {
        tiramisu::computation* cpt = this->sched_graph.begin()->first;
        while (cpt->get_successor() != NULL) {
            cpt = cpt->get_successor();
        }
        return cpt;
    } else {
        DEBUG(3, tiramisu::str_dump("this->is_sched_graph_tree(): false."));
        return NULL;
    }
}

/**
* Return true if the computation \p comp should be vectorized
* at the loop level \p lev.
*/
bool function::should_vectorize(const std::string &comp, int lev) const
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    assert(!comp.empty());
    assert(lev >= 0);

    bool found = false;

    DEBUG(10, tiramisu::str_dump("Checking if the computation " + comp +
                                 " should be vectorized" +
                                 " at the loop level " + std::to_string(lev)));

    DEBUG_INDENT(4);

    for (const auto &pd : this->vector_dimensions)
    {
        DEBUG(10, tiramisu::str_dump("Comparing " + comp + " to " + std::get<0>(pd)));
        DEBUG(10, tiramisu::str_dump(std::get<0>(pd) + " is marked for vectorization at level " + std::to_string(std::get<1>(pd))));

        if ((std::get<0>(pd) == comp) && (std::get<1>(pd) == lev))
            found = true;
    }

    std::string str = "Dimension " + std::to_string(lev) +
                      (found ? " should" : " should not")
                       + " be vectorized.";
    DEBUG(10, tiramisu::str_dump(str));

    DEBUG_INDENT(-4);
    DEBUG_INDENT(-4);

    return found;
}

bool function::should_distribute(const std::string &comp, int lev) const
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    assert(!comp.empty());
    assert(lev >= 0);

    bool found = false;

    DEBUG(10, tiramisu::str_dump("Checking if the computation " + comp +
                                 " should be distributed" +
                                 " at the loop level " + std::to_string(lev)));

    for (const auto &pd : this->distributed_dimensions)
    {
        DEBUG(10, tiramisu::str_dump("Comparing " + comp + " to " + std::get<0>(pd)));
        DEBUG(10, tiramisu::str_dump(std::get<0>(pd) + " is marked for distribution at level " + std::to_string(std::get<1>(pd))));

        if ((std::get<0>(pd) == comp) && (std::get<1>(pd) == lev))
            found = true;
    }

    std::string str = "Dimension " + std::to_string(lev) +
                      (found ? " should" : " should not")
                      + " be distributed.";
    DEBUG(10, tiramisu::str_dump(str));

    DEBUG_INDENT(-4);

    return found;
}

bool tiramisu::function::needs_rank_call() const
{
    return _needs_rank_call;
}

void function::set_context_set(isl_set *context)
{
    assert((context != NULL) && "Context is NULL");

    this->context_set = context;
}

void function::set_context_set(const std::string &context_str)
{
    assert((!context_str.empty()) && "Context string is empty");

    this->context_set = isl_set_read_from_str(this->get_isl_ctx(), context_str.c_str());
    assert((context_set != NULL) && "Context set is NULL");
}

void function::add_context_constraints(const std::string &context_str)
{
    assert((!context_str.empty()) && "Context string is empty");

    if (this->context_set != NULL)
    {
        this->context_set =
            isl_set_intersect(this->context_set,
                              isl_set_read_from_str(this->get_isl_ctx(), context_str.c_str()));
    }
    else
    {
        this->context_set = isl_set_read_from_str(this->get_isl_ctx(), context_str.c_str());
    }
    assert((context_set != NULL) && "Context set is NULL");
}

/**
  * Set the iterator names of the function.
  */
void function::set_iterator_names(const std::vector<std::string> &iteratorNames)
{
    iterator_names = iteratorNames;
}

/**
  * Add an iterator to the function.
  */
void function::add_iterator_name(const std::string &iteratorName)
{
    iterator_names.push_back(iteratorName);
}

/**
  * Generate an object file.  This object file will contain the compiled
  * function.
  * \p obj_file_name indicates the name of the generated file.
  * \p os indicates the target operating system.
  * \p arch indicates the architecture of the target (the instruction set).
  * \p bits indicate the bit-width of the target machine.
  *    must be 0 for unknown, or 32 or 64.
  * For a full list of supported value for \p os and \p arch please
  * check the documentation of Halide::Target
  * (http://halide-lang.org/docs/struct_halide_1_1_target.html).
  * If the machine parameters are not supplied, it will detect one automatically.
  */
// @{
void function::gen_halide_obj(const std::string &obj_file_name) const
{
    Halide::Target target = Halide::get_host_target();
    gen_halide_obj(obj_file_name, target.os, target.arch, target.bits);
}
// @}

/**
 * A pass to rename computations.
 * Computation that are defined multiple times need to be renamed, because
 * those computations in general have different expressions and the code
 * generator expects that computations that have the same name always have
 * the same expression and access relation. So we should rename them to avoid
 * any ambiguity for the code generator.
 *
 */
void tiramisu::function::rename_computations()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    // Computations that have been defined multiple times should
    // be renamed. ISL code generator expects computations with the
    // same name to have the same expressions and the same access
    // relation. So, "update" computations that have the same name
    // but have different expressions should be renamed first so
    // that we can use the original code generator without any
    // modification.
    for (auto const comp : this->get_computations())
    {
        std::vector<tiramisu::computation *> same_name_computations =
            this->get_computation_by_name(comp->get_name());

        int i = 0;

        if (same_name_computations.size() > 1)
            for (auto c : same_name_computations)
            {
                std::string new_name = "_" + c->get_name() + "_update_" + std::to_string(i);
                c->rename_computation(new_name);
                i++;
            }
    }

    DEBUG(3, tiramisu::str_dump("After renaming the computations."));
    DEBUG(3, this->dump(false));

    DEBUG_INDENT(-4);
}

/**
  * Generate an isl AST for the function.
  */
void function::gen_isl_ast()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    // Check that time_processor representation has already been computed,
    assert(this->get_trimmed_time_processor_domain() != NULL);
    assert(this->get_aligned_identity_schedules() != NULL);

    isl_ctx *ctx = this->get_isl_ctx();
    assert(ctx != NULL);
    isl_ast_build *ast_build;

    // Rename updates so that they have different names because
    // the code generator expects each unique name to have
    // an expression, different computations that have the same
    // name cannot have different expressions.
    this->rename_computations();

    if (this->get_program_context() == NULL)
    {
        ast_build = isl_ast_build_alloc(ctx);
    }
    else
    {
        ast_build = isl_ast_build_from_context(isl_set_copy(this->get_program_context()));
    }

    isl_options_set_ast_build_atomic_upper_bound(ctx, 1);
    isl_options_get_ast_build_exploit_nested_bounds(ctx);
    isl_options_set_ast_build_group_coscheduled(ctx, 1);

    ast_build = isl_ast_build_set_after_each_for(ast_build, &tiramisu::for_code_generator_after_for,
                NULL);
    ast_build = isl_ast_build_set_at_each_domain(ast_build, &tiramisu::generator::stmt_code_generator,
                this);

    // Set iterator names
    isl_id_list *iterators = isl_id_list_alloc(ctx, this->get_iterator_names().size());
    if (this->get_iterator_names().size() > 0)
    {
        std::string name = generate_new_variable_name();
        isl_id *id = isl_id_alloc(ctx, name.c_str(), NULL);
        iterators = isl_id_list_add(iterators, id);

        for (int i = 0; i < this->get_iterator_names().size(); i++)
        {
            name = this->get_iterator_names()[i];
            id = isl_id_alloc(ctx, name.c_str(), NULL);
            iterators = isl_id_list_add(iterators, id);

            name = generate_new_variable_name();
            id = isl_id_alloc(ctx, name.c_str(), NULL);
            iterators = isl_id_list_add(iterators, id);
        }

        ast_build = isl_ast_build_set_iterators(ast_build, iterators);
    }

    // Intersect the iteration domain with the domain of the schedule.
    isl_union_map *umap =
        isl_union_map_intersect_domain(
            isl_union_map_copy(this->get_aligned_identity_schedules()),
            isl_union_set_copy(this->get_trimmed_time_processor_domain()));

    DEBUG(3, tiramisu::str_dump("Schedule:", isl_union_map_to_str(this->get_schedule())));
    DEBUG(3, tiramisu::str_dump("Iteration domain:",
                                isl_union_set_to_str(this->get_iteration_domain())));
    DEBUG(3, tiramisu::str_dump("Trimmed Time-Processor domain:",
                                isl_union_set_to_str(this->get_trimmed_time_processor_domain())));
    DEBUG(3, tiramisu::str_dump("Trimmed Time-Processor aligned identity schedule:",
                                isl_union_map_to_str(this->get_aligned_identity_schedules())));
    DEBUG(3, tiramisu::str_dump("Identity schedule intersect trimmed Time-Processor domain:",
                                isl_union_map_to_str(umap)));
    DEBUG(3, tiramisu::str_dump("\n"));

    this->ast = isl_ast_build_node_from_schedule_map(ast_build, umap);

    isl_ast_build_free(ast_build);

    DEBUG_INDENT(-4);
}

void tiramisu::function::allocate_and_map_buffers_automatically()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    DEBUG(10, tiramisu::str_dump("Computing live-out computations."));
    // Compute live-in and live-out buffers
    std::vector<tiramisu::computation *> liveout = this->get_live_out_computations();
    DEBUG(10, tiramisu::str_dump("Allocating/Mapping buffers for live-out computations."));
    for (auto &comp: liveout)
        if (comp->get_automatically_allocated_buffer() == NULL)
            comp->allocate_and_map_buffer_automatically(a_output);

    DEBUG(10, tiramisu::str_dump("Computing live-in computations."));
    // Compute live-in and live-out buffers
    std::vector<tiramisu::computation *> livein =
            this->get_live_in_computations();
    DEBUG(10, tiramisu::str_dump("Allocating/Mapping buffers for live-in computations."));
    // Allocate each live-in computation that is not also live-out (we check that
    // by checking that it was not allocated yet)
    for (auto &comp: livein)
        if (comp->get_automatically_allocated_buffer() == NULL)
            comp->allocate_and_map_buffer_automatically(a_input);

    DEBUG(10, tiramisu::str_dump("Allocating/Mapping buffers for non live-in and non live-out computations."));
    // Allocate the buffers automatically for non inline computations
    // Allocate each computation that is not live-in or live-out (we check that
    // by checking that it was not allocated)
    for (int b = 0; b < this->body.size(); b++)
    {
        DEBUG(3, tiramisu::str_dump("Inline " + this->body[b]->get_name() + " " + std::to_string(this->body[b]->is_inline_computation())));
        if (this->body[b]->is_inline_computation()) {
            DEBUG(3, tiramisu::str_dump("Skipping inline computation " + this->body[b]->get_name()));
            continue;
        }
        DEBUG(10, tiramisu::str_dump("Allocating/Mapping buffers for " + this->body[b]->get_name()));
        if ((this->body[b]->get_expr().get_expr_type() == tiramisu::e_op))
        {
            if (this->body[b]->get_expr().get_op_type() != tiramisu::o_allocate)
            {
                if (this->body[b]->get_automatically_allocated_buffer() == NULL)
                    this->body[b]->allocate_and_map_buffer_automatically(a_temporary);
            }
        }
        else
        {
            if (this->body[b]->get_automatically_allocated_buffer() == NULL) {
                this->body[b]->allocate_and_map_buffer_automatically(a_temporary);
            }
        }
    }

    DEBUG_INDENT(-4);
}

std::string tiramisu::function::get_gpu_thread_iterator(const std::string &comp, int lev0) const
{
    assert(!comp.empty());
    assert(lev0 >= 0);

    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    std::string res = "";

    for (const auto &pd : this->gpu_thread_dimensions)
    {
        if ((pd.first == comp) && ((std::get<0>(pd.second) == lev0) || (std::get<1>(pd.second) == lev0) ||
                                   (std::get<2>(pd.second) == lev0)))
        {
            if (lev0 == std::get<0>(pd.second))
            {
                res = ".__thread_id_z";
            }
            else if (lev0 == std::get<1>(pd.second))
            {
                res = ".__thread_id_y";
            }
            else if (lev0 == std::get<2>(pd.second))
            {
                res = ".__thread_id_x";
            }
            else
            {
                ERROR("Level not mapped to GPU.", true);
            }

            std::string str = "Dimension " + std::to_string(lev0) +
                              " should be mapped to iterator " + res;
            str = str + ". It was compared against: " + std::to_string(std::get<0>(pd.second)) +
                  ", " + std::to_string(std::get<1>(pd.second)) + " and " +
                  std::to_string(std::get<2>(pd.second));
            DEBUG(3, tiramisu::str_dump(str));
        }
    }

    DEBUG_INDENT(-4);
    return res;
}

std::string tiramisu::function::get_gpu_block_iterator(const std::string &comp, int lev0) const
{
    assert(!comp.empty());
    assert(lev0 >= 0);

    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    std::string res = "";;

    for (const auto &pd : this->gpu_block_dimensions)
    {
        if ((pd.first == comp) && ((std::get<0>(pd.second) == lev0) || (std::get<1>(pd.second) == lev0) ||
                                   (std::get<2>(pd.second) == lev0)))
        {
            if (lev0 == std::get<0>(pd.second))
            {
                res = ".__block_id_z";
            }
            else if (lev0 == std::get<1>(pd.second))
            {
                res = ".__block_id_y";
            }
            else if (lev0 == std::get<2>(pd.second))
            {
                res = ".__block_id_x";
            }
            else
            {
                ERROR("Level not mapped to GPU.", true);
            }

            std::string str = "Dimension " + std::to_string(lev0) +
                              " should be mapped to iterator " + res;
            str = str + ". It was compared against: " + std::to_string(std::get<0>(pd.second)) +
                  ", " + std::to_string(std::get<1>(pd.second)) + " and " +
                  std::to_string(std::get<2>(pd.second));
            DEBUG(3, tiramisu::str_dump(str));
        }
    }

    DEBUG_INDENT(-4);
    return res;
}

bool tiramisu::function::should_unroll(const std::string &comp, int lev0) const
{
    assert(!comp.empty());
    assert(lev0 >= 0);

    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    bool found = false;
    for (const auto &pd : this->unroll_dimensions)
    {
        if ((std::get<0>(pd) == comp) && (std::get<1>(pd) == lev0))
        {
            found = true;
        }
    }

    std::string str = "Dimension " + std::to_string(lev0) +
                      (found ? " should" : " should not") +
                      " be unrolled.";
    DEBUG(10, tiramisu::str_dump(str));

    DEBUG_INDENT(-4);
    return found;
}

bool tiramisu::function::should_map_to_gpu_block(const std::string &comp, int lev0) const
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    assert(!comp.empty());
    assert(lev0 >= 0);

    bool found = false;
    for (const auto &pd : this->gpu_block_dimensions)
    {
        if ((pd.first == comp) && ((std::get<0>(pd.second) == lev0) || (std::get<1>(pd.second) == lev0) ||
                                   (std::get<2>(pd.second) == lev0)))
        {
            found = true;
        }
    }

    std::string str = "Dimension " + std::to_string(lev0) +
                      (found ? " should" : " should not")
                      + " be mapped to GPU block.";
    DEBUG(10, tiramisu::str_dump(str));

    DEBUG_INDENT(-4);
    return found;
}

bool tiramisu::function::should_map_to_gpu_thread(const std::string &comp, int lev0) const
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    assert(!comp.empty());
    assert(lev0 >= 0);

    bool found = false;
    for (const auto &pd : this->gpu_thread_dimensions)
    {
        if ((pd.first == comp) && ((std::get<0>(pd.second) == lev0) || (std::get<1>(pd.second) == lev0) ||
                                   (std::get<2>(pd.second) == lev0)))
        {
            found = true;
        }
    }

    std::string str = "Dimension " + std::to_string(lev0) +
                      (found ? " should" : " should not")
                      + " be mapped to GPU thread.";
    DEBUG(10, tiramisu::str_dump(str));

    DEBUG_INDENT(-4);
    return found;
}

int tiramisu::function::get_max_identity_schedules_range_dim() const
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    int max_dim = 0;
    for (const auto &comp : this->get_computations())
    {
        isl_map *sched = comp->gen_identity_schedule_for_time_space_domain();
        int m = isl_map_dim(sched, isl_dim_out);
        max_dim = std::max(max_dim, m);
    }

    DEBUG_INDENT(-4);

    return max_dim;
}

int tiramisu::function::get_max_iteration_domains_dim() const
{
    int max_dim = 0;
    for (const auto &comp : this->get_computations())
    {
        isl_set *domain = comp->get_iteration_domain();
        int m = isl_set_dim(domain, isl_dim_set);
        max_dim = std::max(max_dim, m);
    }

    return max_dim;
}

int tiramisu::function::get_max_schedules_range_dim() const
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    int max_dim = 0;
    for (const auto &comp : this->get_computations())
    {
        isl_map *sched = comp->get_schedule();
        int m = isl_map_dim(sched, isl_dim_out);
        max_dim = std::max(max_dim, m);
    }

    DEBUG_INDENT(-4);

    return max_dim;
}

isl_union_map *tiramisu::function::get_aligned_identity_schedules() const
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    isl_union_map *result;
    isl_space *space;

    if (this->body.empty() == false)
    {
        space = isl_map_get_space(this->body[0]->gen_identity_schedule_for_time_space_domain());
    }
    else
    {
        return NULL;
    }
    assert(space != NULL);
    result = isl_union_map_empty(space);

    int max_dim = this->get_max_identity_schedules_range_dim();

    for (const auto &comp : this->get_computations())
    {
        if (comp->should_schedule_this_computation())
        {
            isl_map *sched = comp->gen_identity_schedule_for_time_space_domain();
            DEBUG(3, tiramisu::str_dump("Identity schedule for time space domain: ", isl_map_to_str(sched)));
            assert((sched != NULL) && "Identity schedule could not be computed");
            sched = isl_map_align_range_dims(sched, max_dim);
            result = isl_union_map_union(result, isl_union_map_from_map(sched));
        }
    }

    DEBUG_INDENT(-4);
    DEBUG(3, tiramisu::str_dump("End of function"));

    return result;
}

void tiramisu::function::align_schedules()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    int max_dim = this->get_max_schedules_range_dim();

    for (auto &comp : this->get_computations())
    {
        isl_map *dup_sched = comp->get_schedule();
        assert((dup_sched != NULL) && "Schedules should be set before calling align_schedules");
        dup_sched = isl_map_align_range_dims(dup_sched, max_dim);
        comp->set_schedule(dup_sched);
        comp->name_unnamed_time_space_dimensions();
    }

    DEBUG_INDENT(-4);
    DEBUG(3, tiramisu::str_dump("End of function"));
}

void tiramisu::function::add_invariant(tiramisu::constant invar)
{
    invariants.push_back(invar);
}

void tiramisu::function::add_computation(computation *cpt)
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    assert(cpt != NULL);

    this->body.push_back(cpt);
    if (cpt->should_schedule_this_computation())
        this->starting_computations.insert(cpt);

    DEBUG_INDENT(-4);
}

void tiramisu::function::dump(bool exhaustive) const
{
    if (ENABLE_DEBUG)
    {
        std::cout << "\n\nFunction \"" << this->name << "\"" << std::endl << std::endl;

        if (this->function_arguments.size() > 0)
        {
            std::cout << "Function arguments (tiramisu buffers):" << std::endl;
            for (const auto &buf : this->function_arguments)
            {
                buf->dump(exhaustive);
            }
            std::cout << std::endl;
        }

        if (this->invariants.size() > 0)
        {
            std::cout << "Function invariants:" << std::endl;
            for (const auto &inv : this->invariants)
            {
                inv.dump(exhaustive);
            }
            std::cout << std::endl;
        }

        if (this->get_program_context() != NULL)
        {
            std::cout << "Function context set: "
                      << isl_set_to_str(this->get_program_context())
                      << std::endl;
        }

        std::cout << "Body " << std::endl;
        for (const auto &cpt : this->body)
        {
            cpt->dump();
        }
        std::cout << std::endl;

        if (this->halide_stmt.defined())
        {
            std::cout << "Halide stmt " << this->halide_stmt << std::endl;
        }

        std::cout << "Buffers" << std::endl;
        for (const auto &buf : this->buffers_list)
        {
            std::cout << "Buffer name: " << buf.second->get_name() << std::endl;
            buf.second->dump(false);
        }

        std::cout << std::endl << std::endl;
    }
}

void tiramisu::function::dump_iteration_domain() const
{
    if (ENABLE_DEBUG)
    {
        tiramisu::str_dump("\nIteration domain:\n");
        for (const auto &cpt : this->body)
        {
            cpt->dump_iteration_domain();
        }
        tiramisu::str_dump("\n");
    }
}

void tiramisu::function::dump_schedule() const
{
    if (ENABLE_DEBUG)
    {
        tiramisu::str_dump("\nDumping schedules of the function " + this->get_name() + " :\n");

        for (const auto &cpt : this->body)
        {
            cpt->dump_schedule();
        }

        std::cout << "Parallel dimensions: ";
        for (const auto &par_dim : parallel_dimensions)
        {
            std::cout << par_dim.first << "(" << par_dim.second << ") ";
        }

        std::cout << std::endl;

        std::cout << "Vector dimensions: ";
        for (const auto &vec_dim : vector_dimensions)
        {
            std::cout << std::get<0>(vec_dim) << "(" << std::get<1>(vec_dim) << ") ";
        }

        std::cout << std::endl << std::endl << std::endl;
    }
}

void tiramisu::function::set_arguments(const std::vector<tiramisu::buffer *> &buffer_vec)
{
    this->function_arguments = buffer_vec;
    // Implicit buffers are set to type a_temporary by default. Change them to
    // a_input or a_output, so that they don't get autoallocated.
    for (auto &buffer : buffer_vec)
    {
        assert((buffer != nullptr) && "Buffer argument is null!");
        if (buffer->get_argument_type() == a_temporary)
        {
            // Determine if it's an input function.
            // If there are any scheduled computation that uses this buffer,
            // buffer is marked as output.
            bool is_input = true;
            for (auto const &comp : this->body)
            {
                if (comp->get_buffer() == buffer
                    && comp->should_schedule_this_computation())
                {
                    is_input = false;
                    break;
                }
            }
            DEBUG(3, tiramisu::str_dump("Setting type of buffer "
                     + buffer->get_name() + " to "
                     + (is_input ? "input" : "output")));
            buffer->set_argument_type(is_input ? a_input : a_output);
        }
    }
}

void tiramisu::function::add_vector_dimension(std::string stmt_name, int vec_dim, int vector_length)
{
    assert(vec_dim >= 0);
    assert(!stmt_name.empty());

    this->vector_dimensions.push_back(std::make_tuple(stmt_name, vec_dim, vector_length));
}

void tiramisu::function::add_distributed_dimension(std::string stmt_name, int dim)
{
    assert(dim >= 0);
    assert(!stmt_name.empty());

    this->distributed_dimensions.push_back({stmt_name, dim});
}

void tiramisu::function::add_parallel_dimension(std::string stmt_name, int vec_dim)
{
    assert(vec_dim >= 0);
    assert(!stmt_name.empty());

    this->parallel_dimensions.push_back({stmt_name, vec_dim});
}

void tiramisu::function::add_unroll_dimension(std::string stmt_name, int level, int factor)
{
    assert(level >= 0);
    assert(!stmt_name.empty());
    assert(factor >= 0);

    this->unroll_dimensions.push_back(std::make_tuple(stmt_name, level, factor));
}

void tiramisu::function::add_gpu_block_dimensions(std::string stmt_name, int dim0,
        int dim1, int dim2)
{
    assert(!stmt_name.empty());
    assert(dim0 >= 0);
    // dim1 and dim2 can be -1 if not set.

    this->gpu_block_dimensions.push_back(
        std::pair<std::string, std::tuple<int, int, int>>(
            stmt_name,
            std::tuple<int, int, int>(dim0, dim1, dim2)));
}

void tiramisu::function::add_gpu_thread_dimensions(std::string stmt_name, int dim0,
        int dim1, int dim2)
{
    assert(!stmt_name.empty());
    assert(dim0 >= 0);
    // dim1 and dim2 can be -1 if not set.

    this->gpu_thread_dimensions.push_back(
        std::pair<std::string, std::tuple<int, int, int>>(
            stmt_name,
            std::tuple<int, int, int>(dim0, dim1, dim2)));
}

isl_union_set *tiramisu::function::get_trimmed_time_processor_domain() const
{
    DEBUG_FCT_NAME(10);
    DEBUG_INDENT(4);

    isl_union_set *result = NULL;
    isl_space *space = NULL;
    if (!this->body.empty())
    {
        space = isl_set_get_space(this->body[0]->get_trimmed_time_processor_domain());
    }
    else
    {
        DEBUG_INDENT(-4);
        return NULL;
    }
    assert(space != NULL);

    result = isl_union_set_empty(space);

    for (const auto &cpt : this->body)
    {
        if (cpt->should_schedule_this_computation())
        {
            isl_set *cpt_iter_space = isl_set_copy(cpt->get_trimmed_time_processor_domain());
            result = isl_union_set_union(isl_union_set_from_set(cpt_iter_space), result);
        }
    }

    DEBUG_INDENT(-4);

    return result;
}

isl_union_set *tiramisu::function::get_time_processor_domain() const
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    isl_union_set *result = NULL;
    isl_space *space = NULL;

    if (!this->body.empty())
    {
        space = isl_set_get_space(this->body[0]->get_time_processor_domain());
    }
    else
    {
        return NULL;
    }

    assert(space != NULL);
    result = isl_union_set_empty(space);

    for (const auto &cpt : this->body)
    {
        if (cpt->should_schedule_this_computation())
        {
            isl_set *cpt_iter_space = isl_set_copy(cpt->get_time_processor_domain());
            result = isl_union_set_union(isl_union_set_from_set(cpt_iter_space), result);
        }
    }

    DEBUG_INDENT(-4);

    return result;
}

isl_union_set *tiramisu::function::get_iteration_domain() const
{
    isl_union_set *result = NULL;
    isl_space *space = NULL;

    if (!this->body.empty())
    {
        space = isl_set_get_space(this->body[0]->get_iteration_domain());
    }
    else
    {
        return NULL;
    }

    assert(space != NULL);
    result = isl_union_set_empty(space);

    for (const auto &cpt : this->body)
    {
        if (cpt->should_schedule_this_computation())
        {
            isl_set *cpt_iter_space = isl_set_copy(cpt->get_iteration_domain());
            result = isl_union_set_union(isl_union_set_from_set(cpt_iter_space), result);
        }
    }

    return result;
}

isl_union_map *tiramisu::function::get_schedule() const
{
    isl_union_map *result = NULL;
    isl_space *space = NULL;

    if (!this->body.empty())
    {
        space = isl_map_get_space(this->body[0]->get_schedule());
    }
    else
    {
        return NULL;
    }

    assert(space != NULL);
    result = isl_union_map_empty(isl_space_copy(space));

    for (const auto &cpt : this->body)
    {
        isl_map *m = isl_map_copy(cpt->get_schedule());
        result = isl_union_map_union(isl_union_map_from_map(m), result);
    }

    result = isl_union_map_intersect_domain(result, this->get_iteration_domain());

    return result;
}

isl_union_map *tiramisu::function::get_trimmed_schedule() const
{
    isl_union_map *result = NULL;
    isl_space *space = NULL;

    if (!this->body.empty())
    {
        space = isl_map_get_space(this->body[0]->get_trimmed_union_of_schedules());
    }
    else
    {
        return NULL;
    }

    assert(space != NULL);
    result = isl_union_map_empty(isl_space_copy(space));

    for (const auto &cpt : this->body)
    {
        isl_map *m = isl_map_copy(cpt->get_trimmed_union_of_schedules());
        result = isl_union_map_union(isl_union_map_from_map(m), result);
    }

    result = isl_union_map_intersect_domain(result, this->get_iteration_domain());

    return result;
}

void tiramisu::function::lift_dist_comps() {
    for (std::vector<tiramisu::computation *>::iterator comp = body.begin(); comp != body.end(); comp++) {
        if ((*comp)->is_send() || (*comp)->is_recv() || (*comp)->is_wait() || (*comp)->is_send_recv()) {
            xfer_prop chan = static_cast<tiramisu::communicator *>(*comp)->get_xfer_props();
            if (chan.contains_attr(MPI)) {
                lift_mpi_comp(*comp);
            } else {
                ERROR("Can only lift MPI library calls", 0);
            }
        }
    }
}

void tiramisu::function::lift_mpi_comp(tiramisu::computation *comp) {
    if (comp->is_send()) {
        send *s = static_cast<send *>(comp);
        tiramisu::expr num_elements(s->get_num_elements());
        tiramisu::expr send_type(s->get_xfer_props().get_dtype());
        bool isnonblock = s->get_xfer_props().contains_attr(NONBLOCK);
        // Determine the appropriate number of function args and set ones that we can already know
        s->rhs_argument_idx = 3;
        s->library_call_args.resize(isnonblock ? 5 : 4);
        s->library_call_args[0] = tiramisu::expr(tiramisu::o_cast, p_int32, num_elements);
        s->library_call_args[1] = tiramisu::expr(tiramisu::o_cast, p_int32, s->get_dest());
        s->library_call_args[2] = tiramisu::expr(tiramisu::o_cast, p_int32, s->get_msg_tag());
        if (isnonblock) {
            // This additional RHS argument is to the request buffer. It is really more of a side effect.
            s->wait_argument_idx = 4;
        }
    } else if (comp->is_recv()) {
        recv *r = static_cast<recv *>(comp);
        send *s = r->get_matching_send();
        tiramisu::expr num_elements(r->get_num_elements());
        tiramisu::expr recv_type(s->get_xfer_props().get_dtype());
        bool isnonblock = r->get_xfer_props().contains_attr(NONBLOCK);
        // Determine the appropriate number of function args and set ones that we can already know
        r->lhs_argument_idx = 3;
        r->library_call_args.resize(isnonblock ? 5 : 4);
        r->library_call_args[0] = tiramisu::expr(tiramisu::o_cast, p_int32, num_elements);
        r->library_call_args[1] = tiramisu::expr(tiramisu::o_cast, p_int32, r->get_src());
        r->library_call_args[2] = tiramisu::expr(tiramisu::o_cast, p_int32, r->get_msg_tag().is_defined() ?
                                                                            r->get_msg_tag() : s->get_msg_tag());
        r->lhs_access_type = tiramisu::o_address_of;
        if (isnonblock) {
            // This RHS argument is to the request buffer. It is really more of a side effect.
          r->wait_argument_idx = 4;
        }
    } else if (comp->is_wait()) {
        wait *w = static_cast<wait *>(comp);
        // Determine the appropriate number of function args and set ones that we can already know
        w->rhs_argument_idx = 0;
        w->library_call_args.resize(1);
        w->library_call_name = "tiramisu_MPI_Wait";
    }
}

void function::gen_ordering_schedules()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    if (this->use_low_level_scheduling_commands)
    {
        DEBUG(3, tiramisu::str_dump("Low level scheduling commands were used."));
        DEBUG(3, tiramisu::str_dump("Discarding high level scheduling commands."));
        return;
    }

    this->dump_sched_graph();

    if(this->is_sched_graph_tree())
    {
        DEBUG(3, tiramisu::str_dump("this->is_sched_graph_tree(): true."));

        std::priority_queue<int> level_to_check;
        std::unordered_map<int, std::deque<computation *>> level_queue;

        auto current_comp = *(this->starting_computations.begin());

        auto init_sched = automatically_allocated;
        init_sched.push_back(current_comp);

        for (auto it = init_sched.begin(); it != init_sched.end() && it + 1 != init_sched.end(); it++)
            (*(it+1))->after_low_level(**it, computation::root_dimension);

        bool comps_remain = true;
        while(comps_remain)
        {
            for (auto &edge: this->sched_graph[current_comp])
            {
                if (level_queue[edge.second].size() == 0)
                    level_to_check.push(edge.second);

                level_queue[edge.second].push_back(edge.first);
            }

            comps_remain = level_to_check.size() > 0;
            // If we haven't exhausted all computations
            if (comps_remain)
            {
                int fuse_level = level_to_check.top();
                auto next_comp = level_queue[fuse_level].front();
                level_queue[fuse_level].pop_front();

                // assert(this->get_max_iteration_domains_dim() > fuse_level);

                next_comp->after_low_level((*current_comp), fuse_level);

                current_comp = next_comp;
                if (level_queue[fuse_level].size() == 0)
                    level_to_check.pop();
            }
        }
    }
    else
    {
        DEBUG(3, tiramisu::str_dump("this->is_sched_graph_tree(): false."));
    }

    DEBUG_INDENT(-4);
}

void function::gen_time_space_domain()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    // Generate the ordering based on calls to .after() and .before().
    this->gen_ordering_schedules();

    this->align_schedules();

    for (auto &comp : this->get_computations())
    {
        comp->gen_time_space_domain();
    }

    DEBUG_INDENT(-4);
}

void tiramisu::function::codegen(const std::vector<tiramisu::buffer *> &arguments, const std::string obj_filename, const bool gen_cuda_stmt)
{
    if (gen_cuda_stmt)
    {
        if(!this->mapping.empty())
        {
            tiramisu::computation* c1 = this->get_first_cpt();
            tiramisu::computation* c2 = this->get_last_cpt();
            Automatic_communication(c1,c2);
        }
        else
            DEBUG(3, tiramisu::str_dump("You must specify the corresponding CPU buffer to each GPU buffer else you should do the communication manually"));
    }
    this->set_arguments(arguments);
    this->lift_dist_comps();
    this->gen_time_space_domain();
    this->gen_isl_ast();
    if (gen_cuda_stmt) {
        this->gen_cuda_stmt();
    }
    this->gen_halide_stmt();
    this->gen_halide_obj(obj_filename);
}

void tiramisu::function::codegen_select_schedule_number(int schedule_number,
                      const std::vector<tiramisu::buffer *> &arguments, const std::string obj_filename, const bool gen_cuda_stmt )
                      {
                          
                      }
void tiramisu::function::codegen_write_potential_schedules(std::string& path_name,
                      const std::vector<tiramisu::buffer *> &arguments, const std::string obj_filename, const bool gen_cuda_stmt )
                      {// test write 2schudles

                          this->save_computation_default_schedules();
                            this->gen_ordering_schedules();
                            this->align_schedules();

                            std::ofstream out(path_name);

                            //out << "chedule1\n";
                            //out <<(isl_union_map_to_str(this->get_schedule())) ;
                           

                            out.close();

                      }

void tiramisu::function::save_computations_levels(){
    for (auto const &graph1 :this->sched_graph){
        
            for(auto const &graph2:graph1.second){
                this->backup_levels.insert({{graph1.first,graph2.first},graph2.second}) ;

            }
    }
}

void tiramisu::function::restore_computations_levels(){
    for (auto const &graph1 :this->sched_graph){
        
            for(auto &graph2:graph1.second){
                
                this->sched_graph[graph1.first][graph2.first] = this->backup_levels[{graph1.first,graph2.first}]  ;
               

            }
    }
}
 

const std::vector<std::string> tiramisu::function::get_invariant_names() const
{
    const std::vector<tiramisu::constant> inv = this->get_invariants();
    std::vector<std::string> inv_str;

    for (int i = 0; i < inv.size(); i++)
        inv_str.push_back(inv[i].get_name());

    return inv_str;
}

void tiramisu::function::performe_full_dependecy_analysis()
{
    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    // aliegn schedules and order schedules
    this->gen_ordering_schedules() ;
    this->align_schedules() ;

    this->save_computation_default_schedules() ;
    this->save_computations_levels() ;

    this->calculate_dep_flow() ;
    
    DEBUG_INDENT(-4);

}

bool tiramisu::function::check_legality_for_function()
{

    DEBUG_FCT_NAME(3);
    DEBUG_INDENT(4);

    assert(this->dep_read_after_write!=NULL) ;

    this->gen_ordering_schedules() ;
    this->align_schedules() ;

    isl_union_map * all_deps = isl_union_map_range_factor_domain(
        isl_union_map_copy(this->dep_read_after_write)) ;

    all_deps = isl_union_map_union(all_deps,
        isl_union_map_range_factor_domain(isl_union_map_copy(this->dep_write_after_read))) ;

    all_deps = isl_union_map_union(all_deps, 
        isl_union_map_range_factor_domain(isl_union_map_copy(this->dep_write_after_write))) ;

    isl_union_map * universe_of_all_deps = isl_union_map_universe(all_deps);

    std::vector<isl_map *> all_basic_maps ;
    

    auto f = [](isl_map * bmap,void * user) { 

        std::vector<isl_map *>& myName = *reinterpret_cast<std::vector<isl_map*>*>(user) ;
     
        myName.push_back(bmap) ;
        return isl_stat_ok;
    };
    
    isl_stat (*fun_ptr)(isl_map * p,void * m) = (f) ;

    isl_union_map_foreach_map(universe_of_all_deps,fun_ptr,(void * ) &all_basic_maps) ;

    isl_set * left_hs = NULL ;
    isl_set * right_hs = NULL ; // hand side

    computation * left_comp = NULL ;
    computation * right_comp = NULL;

    std::string left_computation_name =  "";
    std::string right_computation_name = "";

    


    bool over_all_legality = true ;
    

    for(auto& space_dep:all_basic_maps)
    {

        DEBUG(3, tiramisu::str_dump(" the map of deps is  "+std::string(isl_map_to_str(space_dep))));

        left_hs = isl_map_domain(isl_map_copy(space_dep)) ;
        right_hs = isl_map_range(isl_map_copy(space_dep)) ;


        left_computation_name =  isl_space_get_tuple_name(
            isl_set_get_space(left_hs),isl_dim_set) ;


        right_computation_name =  isl_space_get_tuple_name(
            isl_set_get_space(right_hs),isl_dim_set) ;



        DEBUG(3, tiramisu::str_dump(" checking legality of deps "+left_computation_name+" -> "+right_computation_name));

            //lhs -> rhs dep 
        
        left_comp = this->get_computation_by_name(left_computation_name)[0] ;
        right_comp = this->get_computation_by_name(right_computation_name)[0]  ;


        if(!left_comp->schedules_pair_relation_is_legal(right_comp))
        {
                over_all_legality = false;
                break;
        }
        

    }

    DEBUG_INDENT(-4);

    return over_all_legality ;
}


void tiramisu::function::prepare_schedules_for_legality_checks()
{
    this->align_schedules() ;
    this->gen_ordering_schedules() ;
}

}
