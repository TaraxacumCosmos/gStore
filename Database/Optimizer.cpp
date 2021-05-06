/*=============================================================================
# Filename: Optimizer.cpp
# Author: Yuqi Zhou, Linglin Yang
# Mail: zhouyuqi@pku.edu.cn, fyulingi@gmail.com
=============================================================================*/

#include "Optimizer.h"

Optimizer::Optimizer(KVstore *kv_store,
                     VSTree *vs_tree,
                     Statistics *statistics,
                     TYPE_TRIPLE_NUM *pre2num,
                     TYPE_TRIPLE_NUM *pre2sub,
                     TYPE_TRIPLE_NUM *pre2obj,
                     TYPE_PREDICATE_ID limitID_predicate,
                     TYPE_ENTITY_LITERAL_ID limitID_literal,
                     TYPE_ENTITY_LITERAL_ID limitID_entity,
                     shared_ptr<Transaction> txn
):
    kv_store_(kv_store), statistics(statistics), pre2num_(pre2num),
    pre2sub_(pre2obj),pre2obj_(pre2obj),limitID_predicate_(limitID_predicate),
    limitID_literal_(limitID_literal),limitID_entity_(limitID_entity),
    txn_(std::move(txn))
{
/*
  this->current_basic_query_ = -1; // updated by result_list.size()
  this->basic_query_list_= make_shared<vector<shared_ptr<BasicQuery>>>();

  auto basic_query_vector = sparql_query.getBasicQueryVec();
  for(auto basic_query_pointer :basic_query_vector)
  {
    auto basic_query_shared_pointer = make_shared<BasicQuery>(*basic_query_pointer);
    this->basic_query_list_->push_back(basic_query_shared_pointer);
  }

  this->candidate_plans_ = make_shared<vector<tuple<shared_ptr<BasicQuery>, shared_ptr<vector<QueryPlan>>>>>(basic_query_list_->size());

  this->execution_plan_=make_shared<vector<QueryPlan>>();
  this->result_list_=make_shared<vector<shared_ptr<vector<TYPE_ENTITY_LITERAL_ID>>>>(); // vector<unsigned*>* result_list;
*/

  // TODO: join_cache_ & cardinality_cache_ not implemented yet.
  this->join_cache_ = make_shared<vector<map<shared_ptr<BasicQuery>,vector<shared_ptr<vector<TYPE_ENTITY_LITERAL_ID>>>>>>(); // map(sub-structure, result_list)
  this->cardinality_cache_ = make_shared<vector<map<shared_ptr<BasicQuery>,shared_ptr<vector<TYPE_ENTITY_LITERAL_ID>>>>>(); // map(sub-structure, cardinality), not in statistics
  this->var_descriptors_ = make_shared<vector<VarDescriptor>>();
}


/**
 * generate a var candidate based on its constant edge/neighbour
 * @param edge_info_vector
 * @param edge_table_info_vector
 * @return
 */
tuple<bool, TableContentShardPtr> Optimizer::GenerateColdCandidateList(const shared_ptr<vector<EdgeInfo>>& edge_info_vector,
                                                                       const shared_ptr<vector<EdgeConstantInfo>>& edge_table_info_vector) {

  if(edge_info_vector->empty())
    return make_tuple(false, nullptr);


  auto var_to_filter = (*edge_info_vector)[0].getVarToFilter();

  assert(edge_info_vector->size()==edge_table_info_vector->size());

  auto id_candidate = CandidatesWithConstantEdge(edge_info_vector);

  auto result = make_shared<TableContent>();
  auto id_candidate_vec = id_candidate->getList();
  for(auto var_id: *id_candidate_vec)
  {
    auto record = make_shared<vector<TYPE_ENTITY_LITERAL_ID>>();
    record->push_back(var_id);
    result->push_back(record);
  }
  cout<<" GenerateColdCandidateList result size:"<<result->size()<<endl;
  cout<<"GenerateColdCandidateList:: result_record_len = "<<result->front()->size()<<endl;
  return make_tuple(true,result);
}


shared_ptr<IDList> Optimizer::CandidatesWithConstantEdge(const shared_ptr<vector<EdgeInfo>> &edge_info_vector) const {
  auto id_candidate = make_shared<IDList>();
  for(int i = 0; i<edge_info_vector->size(); i++)
  {
    auto edge_info = (*edge_info_vector)[i];
    TYPE_ENTITY_LITERAL_ID *edge_candidate_list;
    TYPE_ENTITY_LITERAL_ID this_edge_list_len;
    cout<<"edge["<<i<<"] \n\t"<< edge_info.toString() << "\n\t join method:"<<JoinMethodToString(edge_info.join_method_)<<endl;
    switch (edge_info.join_method_) {
      case JoinMethod::s2p: { // Because if we don't add a pair of '{}', the editor will report a error of redefinition
        auto s_var_constant_id = edge_info.s_;
        kv_store_->getpreIDlistBysubID(s_var_constant_id,
                                       edge_candidate_list,
                                       this_edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::s2o: {
        auto s_var_constant_id = edge_info.s_;
        kv_store_->getobjIDlistBysubID(s_var_constant_id,
                                       edge_candidate_list,
                                       this_edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::p2s: {
        auto p_var_constant_id = edge_info.p_;
        kv_store_->getsubIDlistBypreID(p_var_constant_id,
                                       edge_candidate_list,
                                       this_edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::p2o: {
        auto p_var_constant_id = edge_info.p_;
        kv_store_->getobjIDlistBypreID(p_var_constant_id,
                                       edge_candidate_list,
                                       this_edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::o2s: {
        auto o_var_constant_id = edge_info.o_;
        kv_store_->getsubIDlistByobjID(o_var_constant_id,
                                       edge_candidate_list,
                                       this_edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::o2p: {
        auto o_var_constant_id = edge_info.o_;
        kv_store_->getpreIDlistByobjID(o_var_constant_id,
                                       edge_candidate_list,
                                       this_edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::so2p: {
        auto s_var_constant_id = edge_info.s_;
        auto o_var_constant_id = edge_info.o_;
        kv_store_->getpreIDlistBysubIDobjID(s_var_constant_id,
                                            o_var_constant_id,
                                            edge_candidate_list,
                                            this_edge_list_len,
                                            true,
                                            txn_);
        break;
      }
      case JoinMethod::sp2o: {
        auto s_var_constant_id = edge_info.s_;
        auto p_var_constant_id = edge_info.p_;
        kv_store_->getobjIDlistBysubIDpreID(s_var_constant_id,
                                            p_var_constant_id,
                                            edge_candidate_list,
                                            this_edge_list_len,
                                            true,
                                            txn_);
        break;
      }
      case JoinMethod::po2s: {
        auto p_var_constant_id = edge_info.p_;
        auto o_var_constant_id = edge_info.o_;
        kv_store_->getsubIDlistByobjIDpreID(o_var_constant_id,
                                            p_var_constant_id,
                                            edge_candidate_list,
                                            this_edge_list_len,
                                            true,
                                            txn_);
        break;
      }
    }
    cout<<"get "<<this_edge_list_len<<" result in this edge "<<endl;
    UpdateIDList(id_candidate,edge_candidate_list,this_edge_list_len,i > 0);
  }
  return id_candidate;
}

/**
 * add a node to the table
 * @param one_step_join_node_ Describe how to join a node
 * @param intermediate_result The Result in previous step
 * @return bool: the function is done; IntermediateResult: the new IntermediateResult
 */
tuple<bool,TableContentShardPtr> Optimizer::JoinANode(const shared_ptr<OneStepJoinNode>& one_step_join_node_,
                                                      const TableContentShardPtr& table_content_ptr,
                                                      const PositionValueSharedPtr& id_pos_mapping,
                                                      const IDCachesSharePtr& id_caches)
{


  TableContentShardPtr new_intermediate_table = make_shared<TableContent>();

  auto new_id = one_step_join_node_->node_to_join_;
  auto new_id_position = (*id_pos_mapping)[new_id];

  auto edges_info = one_step_join_node_->edges_;

  /* : each record */
  for (auto record_iterator = table_content_ptr->begin();
       record_iterator != table_content_ptr->end();
       record_iterator++) {
    shared_ptr<IDList> record_candidate_list = ExtendRecord(one_step_join_node_,
                 id_pos_mapping,
                 id_caches,
                 new_id,
                 record_iterator);

    /* write to the new table */
    auto record = *record_iterator;
    for (auto new_element:*(record_candidate_list->getList())) {
      auto new_record = make_shared<vector<TYPE_ENTITY_LITERAL_ID>>(*record);
      new_record->push_back(new_element);
      //cout<<"new_record_len="<<new_record->size()<<endl;
      new_intermediate_table->push_back(new_record);
    }
  }

  //cout<<"Optimizer::JoinANode result_record_len = "<<new_intermediate_table->values_->front()->size()<<endl;
  return make_tuple(true,new_intermediate_table);

}

shared_ptr<IDList> Optimizer::ExtendRecord(const shared_ptr<OneStepJoinNode> &one_step_join_node_,
                             const PositionValueSharedPtr &id_pos_mapping,
                             const IDCachesSharePtr &id_caches,
                             TYPE_ENTITY_LITERAL_ID new_id,
                             list<shared_ptr<vector<TYPE_ENTITY_LITERAL_ID>>>::iterator &record_iterator) const {

  auto record_candidate_list= make_shared<IDList>();
  auto record_candidate_prepared = false;
  auto edges_info = one_step_join_node_->edges_;
  /* for each edge */
  for (int edge_i = 0;edge_i<edges_info->size();edge_i++) {
    auto edge_info = (*edges_info)[edge_i];
    auto edge_constant_info = one_step_join_node_->edges_constant_info_->operator[](edge_i);
    if (record_candidate_prepared && record_candidate_list->empty())
      continue;
    TYPE_ENTITY_LITERAL_ID *edge_candidate_list;
    TYPE_ENTITY_LITERAL_ID edge_list_len;
    /* through this edge get candidate */
    switch (edge_info.join_method_) {
      case JoinMethod::s2p: { // Because if we don't add a pair of '{}', the editor will report a error of redefinition
        /* if s is constant, it is edge constraint check, and should not appear in the function*/
        auto s_var_position = (*id_pos_mapping)[edge_info.s_];
        auto s_var_id_this_record = (**record_iterator)[s_var_position];
        kv_store_->getpreIDlistBysubID(s_var_id_this_record,
                                       edge_candidate_list,
                                       edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::s2o: {
        auto s_var_position = (*id_pos_mapping)[edge_info.s_];
        auto s_var_id_this_record = (**record_iterator)[s_var_position];
        kv_store_->getobjIDlistBysubID(s_var_id_this_record,
                                       edge_candidate_list,
                                       edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::p2s: {
        auto p_var_position = (*id_pos_mapping)[edge_info.p_];
        auto p_var_id_this_record = (**record_iterator)[p_var_position];
        kv_store_->getsubIDlistBypreID(p_var_id_this_record,
                                       edge_candidate_list,
                                       edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::p2o: {
        auto p_var_position = (*id_pos_mapping)[edge_info.p_];
        auto p_var_id_this_record = (**record_iterator)[p_var_position];
        kv_store_->getobjIDlistBypreID(p_var_id_this_record,
                                       edge_candidate_list,
                                       edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::o2s: {
        auto o_var_position = (*id_pos_mapping)[edge_info.o_];
        auto o_var_id_this_record = (**record_iterator)[o_var_position];
        kv_store_->getsubIDlistByobjID(o_var_id_this_record,
                                       edge_candidate_list,
                                       edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::o2p: {
        auto o_var_position = (*id_pos_mapping)[edge_info.o_];
        auto o_var_id_this_record = (**record_iterator)[o_var_position];
        kv_store_->getpreIDlistByobjID(o_var_id_this_record,
                                       edge_candidate_list,
                                       edge_list_len,
                                       true,
                                       txn_);
        break;
      }
      case JoinMethod::so2p: {
        TYPE_ENTITY_LITERAL_ID s_var_position,s_var_id_this_record;
        if(edge_constant_info.s_constant_)
          s_var_id_this_record = edge_info.s_;
        else {
          auto s_var_position = (*id_pos_mapping)[edge_info.s_];
          s_var_id_this_record = (**record_iterator)[s_var_position];
        }

        TYPE_ENTITY_LITERAL_ID o_var_position,o_var_id_this_record;
        if(edge_constant_info.o_constant_)
          o_var_id_this_record = edge_info.o_;
        else {
          auto o_var_position = (*id_pos_mapping)[edge_info.o_];
          o_var_id_this_record = (**record_iterator)[o_var_position];
        }

        kv_store_->getpreIDlistBysubIDobjID(s_var_id_this_record,
                                            o_var_id_this_record,
                                            edge_candidate_list,
                                            edge_list_len,
                                            true,
                                            txn_);
        break;
      }
      case JoinMethod::sp2o: {
        TYPE_ENTITY_LITERAL_ID s_var_position,s_var_id_this_record;
        if(edge_constant_info.s_constant_)
          s_var_id_this_record = edge_info.s_;
        else {
          auto s_var_position = (*id_pos_mapping)[edge_info.s_];
          s_var_id_this_record = (**record_iterator)[s_var_position];
        }

        TYPE_ENTITY_LITERAL_ID p_var_position,p_var_id_this_record;
        if(edge_constant_info.p_constant_)
          p_var_id_this_record = edge_info.p_;
        else {
          auto p_var_position = (*id_pos_mapping)[edge_info.p_];
          p_var_id_this_record = (**record_iterator)[p_var_position];
        }
        kv_store_->getobjIDlistBysubIDpreID(s_var_id_this_record,
                                            p_var_id_this_record,
                                            edge_candidate_list,
                                            edge_list_len,
                                            true,
                                            txn_);
        break;
      }
      case JoinMethod::po2s: {
        TYPE_ENTITY_LITERAL_ID o_var_position,o_var_id_this_record;
        if(edge_constant_info.o_constant_)
          o_var_id_this_record = edge_info.o_;
        else {
          auto o_var_position = (*id_pos_mapping)[edge_info.o_];
          o_var_id_this_record = (**record_iterator)[o_var_position];
        }

        TYPE_ENTITY_LITERAL_ID p_var_position,p_var_id_this_record;
        if(edge_constant_info.p_constant_)
          p_var_id_this_record = edge_info.p_;
        else {
          auto p_var_position = (*id_pos_mapping)[edge_info.p_];
          p_var_id_this_record = (**record_iterator)[p_var_position];
        }

        kv_store_->getsubIDlistByobjIDpreID(o_var_id_this_record,
                                            p_var_id_this_record,
                                            edge_candidate_list,
                                            edge_list_len,
                                            true,
                                            txn_);
        break;
      }
    }

    UpdateIDList(record_candidate_list,
                            edge_candidate_list,
                            edge_list_len,
                            record_candidate_prepared);
    delete [] edge_candidate_list;
    /* If candidate is not prepared, then we ues the first edge as candidate */
    record_candidate_prepared = true;
  }

  if(id_caches->find(new_id)!=id_caches->end())
  {
    auto caches_ptr = (*(id_caches->find(new_id))).second;
    record_candidate_list->intersectList(*caches_ptr);
  }

  return record_candidate_list;
}

/**
 * join two table with vars in one_step_join_table
 * @param one_step_join_table: define join vars
 * @param table_a
 * @param table_b
 * @return new table, columns are made up of
 * [ common vars ][ small table vars ][ larger table vars]
 */
tuple<bool, TableContentShardPtr> Optimizer::JoinTwoTable(const shared_ptr<OneStepJoinTable>& one_step_join_table,
                                                          const TableContentShardPtr& table_a,
                                                          const PositionValueSharedPtr& table_a_id_pos,
                                                          const TableContentShardPtr& table_b,
                                                          const PositionValueSharedPtr& table_b_id_pos) {
  if(table_a->empty() || table_b->empty())
    return make_tuple(false,make_shared<TableContent>());

  auto join_nodes = one_step_join_table->public_variables_;
  /* build index in a
   * cost(a) = a log a + b log a
   * build index in b
   * cost(b) = b log b + a log b
   *
   * if a < b
   * cost(b) - cost(a)
   * = b log b - a log a + a log b - b log a
   * > b log b - a log b + a log b - b log a
   * = b (logb - log a)
   * > 0
   *
   * So, we build index in smaller table
   * */
  auto big_table = table_a->size() > table_b->size() ? table_a : table_b;
  auto big_id_pos = table_a->size() > table_b->size() ? table_a_id_pos : table_b_id_pos;

  auto small_table = table_a->size() < table_b->size() ? table_a : table_b;
  auto small_id_pos = table_a->size() < table_b->size() ? table_a_id_pos : table_b_id_pos;

  auto result_table = make_shared<TableContent>();

  auto indexed_result = unordered_map<
      /*key*/ vector<TYPE_ENTITY_LITERAL_ID>,
      /*value*/ shared_ptr<vector<shared_ptr<vector<TYPE_ENTITY_LITERAL_ID>>>>,
      /*hash function*/ container_hash<vector<TYPE_ENTITY_LITERAL_ID>>
  >();
  // # TODO 先建立大表的索引 ，再建立小表的索引
  vector<TYPE_ENTITY_LITERAL_ID> common_variables_position_big;
  vector<TYPE_ENTITY_LITERAL_ID> common_variables_position_small;
  for(auto common_variable:*(one_step_join_table->public_variables_))
  {
    common_variables_position_big.push_back((*big_id_pos)[common_variable]);
    common_variables_position_small.push_back((*small_id_pos)[common_variable]);
  }

  auto common_variables_size = one_step_join_table->public_variables_->size();
  for(const auto& big_record:*(big_table))
  {
    vector<TYPE_ENTITY_LITERAL_ID> result_index(common_variables_size);
    for(auto common_position:common_variables_position_big)
    {
      result_index.push_back((*big_record)[common_position]);
    }
    if(indexed_result.find(result_index)!=indexed_result.end())
    {
      indexed_result[result_index]->push_back(big_record);
    }
    else
    {
      auto tmp_vector = make_shared<vector<shared_ptr<vector<TYPE_ENTITY_LITERAL_ID>>>>();
      tmp_vector->push_back(big_record);
      indexed_result[result_index] = tmp_vector;
    }
  }


  /* Now Do the Matching , first calculate which ids in table small should be add
   * */
  set<TYPE_ENTITY_LITERAL_ID> public_variables;
  for(auto variable_id:*join_nodes)
  {
    public_variables.insert(variable_id);
  }
  vector<TYPE_ENTITY_LITERAL_ID> small_table_inserted_variables_position;
  for(auto b_kv:*small_id_pos)
  {
    if (public_variables.find(b_kv.second)!=public_variables.end())
      continue;
    small_table_inserted_variables_position.push_back(b_kv.first);
  }

  auto result_contents = result_table;
  /* do the matching */
  for(const auto& small_record:*small_table)
  {
    vector<TYPE_ENTITY_LITERAL_ID> result_index(common_variables_size);
    for(auto common_position:common_variables_position_small)
    {
      result_index.push_back((*small_record)[common_position]);
    }
    /* the index is in the big table */
    if(indexed_result.find(result_index)!=indexed_result.end())
    {
      vector<TYPE_ENTITY_LITERAL_ID> small_record_inserted;
      for(auto small_position:small_table_inserted_variables_position)
      {
        small_record_inserted.push_back((*small_record)[small_position]);
      }
      auto matched_content = indexed_result[result_index];
      for(const auto& matched_small_record:*matched_content)
      {
        auto result_record = make_shared<vector<TYPE_ENTITY_LITERAL_ID>>(*matched_small_record);
        for(auto small_inserted_element:small_record_inserted)
        {
          result_record->push_back(small_inserted_element);
        }
        result_contents->push_back(result_record);
      }
    }
    /* if not, ignore */
  }

  return make_tuple(true,result_table);

}

tuple<bool, TableContentShardPtr> Optimizer::ANodeEdgesConstraintFilter(const shared_ptr<OneStepJoinNode>& one_step_join_table,
                                                                        TableContentShardPtr table_content_ptr,
                                                                        const PositionValueSharedPtr& id_pos_mapping,
                                                                        const IDCachesSharePtr& id_caches) {
  auto edge_info_vec = one_step_join_table->edges_;
  auto edge_constant_info_vec = one_step_join_table->edges_constant_info_;
  for(int i=0;i<one_step_join_table->edges_->size();i++)
  {
    auto edge_info = (*edge_info_vec)[i];
    auto edge_constant_info = (*edge_constant_info_vec)[i];
    auto step_result = this->OneEdgeConstraintFilter(edge_info,edge_constant_info,table_content_ptr,id_pos_mapping,id_caches);
    if(get<0>(step_result))
      table_content_ptr = get<1>(step_result);
  }
  return make_tuple(true,table_content_ptr);
}


/**
 * for a table T[a,b,c] if we want to check if ?a always have a edge (?a,p,o)
 * If T is nullptr,
 * @param edge_info
 * @param edge_table_info
 * @param table_content_ptr
 * @return
 */
tuple<bool, TableContentShardPtr> Optimizer::OneEdgeConstraintFilter(EdgeInfo edge_info,
                                                                     EdgeConstantInfo edge_table_info,
                                                                     const TableContentShardPtr& table_content_ptr,
                                                                     const PositionValueSharedPtr& id_pos_mapping,
                                                                     const IDCachesSharePtr& id_caches) {
  TYPE_ENTITY_LITERAL_ID var_to_filter= edge_info.getVarToFilter();
  if(edge_table_info.ConstantToVar(edge_info))
  {

    TYPE_ENTITY_LITERAL_ID *edge_candidate_list;
    TYPE_ENTITY_LITERAL_ID this_edge_list_len;
    switch (edge_info.join_method_) {
      case JoinMethod::s2p: { // Because if we don't add a pair of '{}', the editor will report a error of redefinition
        auto s_var_constant_id = edge_info.s_;
        this->kv_store_->getpreIDlistBysubID(s_var_constant_id,
                                             edge_candidate_list,
                                             this_edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::s2o: {
        auto s_var_constant_id = edge_info.s_;
        this->kv_store_->getobjIDlistBysubID(s_var_constant_id,
                                             edge_candidate_list,
                                             this_edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::p2s: {
        auto p_var_constant_id = edge_info.p_;
        this->kv_store_->getsubIDlistBypreID(p_var_constant_id,
                                             edge_candidate_list,
                                             this_edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::p2o: {
        auto p_var_constant_id = edge_info.p_;
        this->kv_store_->getobjIDlistBypreID(p_var_constant_id,
                                             edge_candidate_list,
                                             this_edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::o2s: {
        auto o_var_constant_id = edge_info.o_;
        this->kv_store_->getsubIDlistByobjID(o_var_constant_id,
                                             edge_candidate_list,
                                             this_edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::o2p: {
        auto o_var_constant_id = edge_info.o_;
        this->kv_store_->getpreIDlistByobjID(o_var_constant_id,
                                             edge_candidate_list,
                                             this_edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::so2p: {
        auto s_var_constant_id = edge_info.s_;
        auto o_var_constant_id = edge_info.o_;
        this->kv_store_->getpreIDlistBysubIDobjID(s_var_constant_id,
                                                  o_var_constant_id,
                                                  edge_candidate_list,
                                                  this_edge_list_len,
                                                  true,
                                                  this->txn_);
        break;
      }
      case JoinMethod::sp2o: {
        auto s_var_constant_id = edge_info.s_;
        auto p_var_constant_id = edge_info.p_;
        this->kv_store_->getobjIDlistBysubIDpreID(s_var_constant_id,
                                                  p_var_constant_id,
                                                  edge_candidate_list,
                                                  this_edge_list_len,
                                                  true,
                                                  this->txn_);
        break;
      }
      case JoinMethod::po2s: {
        auto p_var_constant_id = edge_info.p_;
        auto o_var_constant_id = edge_info.o_;
        this->kv_store_->getsubIDlistByobjIDpreID(o_var_constant_id,
                                                  p_var_constant_id,
                                                  edge_candidate_list,
                                                  this_edge_list_len,
                                                  true,
                                                  this->txn_);
        break;
      }
    }
    auto validate_list = make_shared<vector<TYPE_ENTITY_LITERAL_ID>>(edge_candidate_list,edge_candidate_list+this_edge_list_len);
    delete [] edge_candidate_list;
    return FilterAVariableOnIDList(validate_list,var_to_filter,table_content_ptr,id_pos_mapping);
  }


  auto result_table = make_shared<TableContent>();
  /* : each record */
  for (auto record_iterator = table_content_ptr->begin(); record_iterator != table_content_ptr->end();record_iterator++) {
    TYPE_ENTITY_LITERAL_ID *edge_candidate_list;
    TYPE_ENTITY_LITERAL_ID edge_list_len;
    switch (edge_info.join_method_) {
      case JoinMethod::s2p: { // Because if we don't add a pair of '{}', the editor will report a error of redefinition
        auto s_var_position = (*id_pos_mapping)[edge_info.s_];
        auto s_var_id_this_record = (**record_iterator)[s_var_position];
        this->kv_store_->getpreIDlistBysubID(s_var_id_this_record,
                                             edge_candidate_list,
                                             edge_list_len,
                                             true,
                                             this->txn_);

        break;
      }
      case JoinMethod::s2o: {
        auto s_var_position = (*id_pos_mapping)[edge_info.s_];
        auto s_var_id_this_record = (**record_iterator)[s_var_position];
        this->kv_store_->getobjIDlistBysubID(s_var_id_this_record,
                                             edge_candidate_list,
                                             edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::p2s: {
        auto p_var_position = (*id_pos_mapping)[edge_info.p_];
        auto p_var_id_this_record = (**record_iterator)[p_var_position];
        this->kv_store_->getsubIDlistBypreID(p_var_id_this_record,
                                             edge_candidate_list,
                                             edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::p2o: {
        auto p_var_position = (*id_pos_mapping)[edge_info.p_];
        auto p_var_id_this_record = (**record_iterator)[p_var_position];
        this->kv_store_->getobjIDlistBypreID(p_var_id_this_record,
                                             edge_candidate_list,
                                             edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::o2s: {
        auto o_var_position = (*id_pos_mapping)[edge_info.o_];
        auto o_var_id_this_record = (**record_iterator)[o_var_position];
        this->kv_store_->getsubIDlistByobjID(o_var_id_this_record,
                                             edge_candidate_list,
                                             edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
      case JoinMethod::o2p: {
        auto o_var_position = (*id_pos_mapping)[edge_info.o_];
        auto o_var_id_this_record = (**record_iterator)[o_var_position];
        this->kv_store_->getpreIDlistByobjID(o_var_id_this_record,
                                             edge_candidate_list,
                                             edge_list_len,
                                             true,
                                             this->txn_);
        break;
      }
        /**
         * Notice: s or o may be constant
         */
      case JoinMethod::so2p: {
        TYPE_ENTITY_LITERAL_ID  s_var_id_this_record;
        TYPE_ENTITY_LITERAL_ID  o_var_id_this_record;

        if(edge_table_info.s_constant_)
        {
          s_var_id_this_record = edge_info.s_;
        }
        else{
          auto s_var_position = (*id_pos_mapping)[edge_info.s_];
          s_var_id_this_record = (**record_iterator)[s_var_position];
        }

        if(edge_table_info.o_constant_)
        {
          o_var_id_this_record = edge_info.o_;
        }
        else{
          auto o_var_position = (*id_pos_mapping)[edge_info.o_];
          o_var_id_this_record = (**record_iterator)[o_var_position];
        }

        this->kv_store_->getpreIDlistBysubIDobjID(s_var_id_this_record,
                                                  o_var_id_this_record,
                                                  edge_candidate_list,
                                                  edge_list_len,
                                                  true,
                                                  this->txn_);
        break;
      }
      case JoinMethod::sp2o: {
        TYPE_ENTITY_LITERAL_ID  s_var_id_this_record;
        TYPE_ENTITY_LITERAL_ID  p_var_id_this_record;
        if(edge_table_info.s_constant_)
        {
          s_var_id_this_record = edge_info.s_;
        }
        else{
          auto s_var_position = (*id_pos_mapping)[edge_info.s_];
          s_var_id_this_record = (**record_iterator)[s_var_position];
        }

        if(edge_table_info.p_constant_)
        {
          p_var_id_this_record = edge_info.p_;
        }
        else{
          auto p_var_position = (*id_pos_mapping)[edge_info.p_];
          p_var_id_this_record = (**record_iterator)[p_var_position];
        }

        this->kv_store_->getobjIDlistBysubIDpreID(s_var_id_this_record,
                                                  p_var_id_this_record,
                                                  edge_candidate_list,
                                                  edge_list_len,
                                                  true,
                                                  this->txn_);
        break;
      }
      case JoinMethod::po2s: {
        TYPE_ENTITY_LITERAL_ID  p_var_id_this_record;
        TYPE_ENTITY_LITERAL_ID  o_var_id_this_record;

        if(edge_table_info.o_constant_)
        {
          o_var_id_this_record = edge_info.o_;
        }
        else{
          auto o_var_position = (*id_pos_mapping)[edge_info.o_];
          o_var_id_this_record = (**record_iterator)[o_var_position];
        }

        if(edge_table_info.p_constant_)
        {
          p_var_id_this_record = edge_info.p_;
        }
        else{
          auto p_var_position = (*id_pos_mapping)[edge_info.p_];
          p_var_id_this_record = (**record_iterator)[p_var_position];
        }

        this->kv_store_->getsubIDlistByobjIDpreID(o_var_id_this_record,
                                                  p_var_id_this_record,
                                                  edge_candidate_list,
                                                  edge_list_len,
                                                  true,
                                                  this->txn_);
        break;
      }

    }

    /**
     * Now we get the valid candidate list of var_to_filter
     * do a binary to decide whether the record satisfy this edge
     * */

    auto var_to_filter_position = (*id_pos_mapping)[var_to_filter];
    auto var_to_filter_id_this_record = (**record_iterator)[var_to_filter_position];
    if(binary_search(edge_candidate_list,edge_candidate_list+edge_list_len,var_to_filter_id_this_record))
    {
      result_table->push_back(*record_iterator);
    }
    delete [] edge_candidate_list;
  }

  return make_tuple(true,result_table);

}

tuple<bool, TableContentShardPtr> Optimizer::FilterAVariableOnIDList(const shared_ptr<vector<TYPE_ENTITY_LITERAL_ID>>& candidate_list,
                                                                     TYPE_ENTITY_LITERAL_ID var_id,
                                                                     const TableContentShardPtr& table_content_ptr,
                                                                     const PositionValueSharedPtr& id_posing_mapping) {

  auto new_intermediate_table = make_shared<TableContent>();
  auto var_position = (*id_posing_mapping)[var_id];

  /* : each record */
  for (const auto& record_sp : *table_content_ptr){
    auto var_to_filter_id_this_record=(*record_sp)[var_position];
    if(binary_search(candidate_list->begin(),candidate_list->end(),var_to_filter_id_this_record))
    {
      new_intermediate_table->push_back(record_sp);
    }
  }

  return make_tuple(true,new_intermediate_table);

}

bool Optimizer::CacheConstantCandidates(const shared_ptr<OneStepJoinNode>& one_step, const IDCachesSharePtr& id_caches) {
  auto node_t = one_step->node_to_join_;
  auto edges = one_step->edges_;
  auto edges_constant = one_step->edges_constant_info_;
  for(int i =0 ;i<edges->size();i++) {
    AddConstantCandidates((*edges)[i],(*edges_constant)[i],node_t,id_caches);
  }
  return true;
}

bool Optimizer::AddConstantCandidates(EdgeInfo edge_info,EdgeConstantInfo edge_table_info,TYPE_ENTITY_LITERAL_ID targetID,
                                                                     const IDCachesSharePtr& id_caches)
{
  TYPE_ENTITY_LITERAL_ID *edge_candidate_list;
  TYPE_ENTITY_LITERAL_ID this_edge_list_len;
  switch (edge_info.join_method_) {
    case JoinMethod::s2p: { // Because if we don't add a pair of '{}', the editor will report a error of redefinition
      auto s_var_constant_id = edge_info.s_;
      this->kv_store_->getpreIDlistBysubID(s_var_constant_id,
                                           edge_candidate_list,
                                           this_edge_list_len,
                                           true,
                                           this->txn_);
      break;
    }
    case JoinMethod::s2o: {
      auto s_var_constant_id = edge_info.s_;
      this->kv_store_->getobjIDlistBysubID(s_var_constant_id,
                                           edge_candidate_list,
                                           this_edge_list_len,
                                           true,
                                           this->txn_);
      break;
    }
    case JoinMethod::p2s: {
      auto p_var_constant_id = edge_info.p_;
      this->kv_store_->getsubIDlistBypreID(p_var_constant_id,
                                           edge_candidate_list,
                                           this_edge_list_len,
                                           true,
                                           this->txn_);
      break;
    }
    case JoinMethod::p2o: {
      auto p_var_constant_id = edge_info.p_;
      this->kv_store_->getobjIDlistBypreID(p_var_constant_id,
                                           edge_candidate_list,
                                           this_edge_list_len,
                                           true,
                                           this->txn_);
      break;
    }
    case JoinMethod::o2s: {
      auto o_var_constant_id = edge_info.o_;
      this->kv_store_->getsubIDlistByobjID(o_var_constant_id,
                                           edge_candidate_list,
                                           this_edge_list_len,
                                           true,
                                           this->txn_);
      break;
    }
    case JoinMethod::o2p: {
      auto o_var_constant_id = edge_info.o_;
      this->kv_store_->getpreIDlistByobjID(o_var_constant_id,
                                           edge_candidate_list,
                                           this_edge_list_len,
                                           true,
                                           this->txn_);
      break;
    }
    case JoinMethod::so2p: {
      auto s_var_constant_id = edge_info.s_;
      auto o_var_constant_id = edge_info.o_;
      this->kv_store_->getpreIDlistBysubIDobjID(s_var_constant_id,
                                                o_var_constant_id,
                                                edge_candidate_list,
                                                this_edge_list_len,
                                                true,
                                                this->txn_);
      break;
    }
    case JoinMethod::sp2o: {
      auto s_var_constant_id = edge_info.s_;
      auto p_var_constant_id = edge_info.p_;
      this->kv_store_->getobjIDlistBysubIDpreID(s_var_constant_id,
                                                p_var_constant_id,
                                                edge_candidate_list,
                                                this_edge_list_len,
                                                true,
                                                this->txn_);
      break;
    }
    case JoinMethod::po2s: {
      auto p_var_constant_id = edge_info.p_;
      auto o_var_constant_id = edge_info.o_;
      this->kv_store_->getsubIDlistByobjIDpreID(o_var_constant_id,
                                                p_var_constant_id,
                                                edge_candidate_list,
                                                this_edge_list_len,
                                                true,
                                                this->txn_);
      break;
    }
  }
  auto validate_list = make_shared<vector<TYPE_ENTITY_LITERAL_ID>>(edge_candidate_list,edge_candidate_list+this_edge_list_len);
  if(id_caches->find(targetID) == id_caches->end())
  {
    auto new_ids = make_shared<IDList>();
    UpdateIDList(new_ids,edge_candidate_list,this_edge_list_len,false);
    (*id_caches)[targetID] = new_ids;
  }
  else
  {
    auto prepared_IDs = (*id_caches->find(targetID)).second;
    UpdateIDList(prepared_IDs,edge_candidate_list,this_edge_list_len,true);
  }
  delete [] edge_candidate_list;

  return true;
}

/**
 * a simple version of Join::update_answer_list
 * @param valid_id_list
 * @param id_list
 * @param id_list_len
 * @param id_list_prepared : valid_id_list is initialed
 */
void
Optimizer::UpdateIDList(const shared_ptr<IDList>& valid_id_list, unsigned* id_list, unsigned id_list_len,bool id_list_prepared)
{
  if(id_list_prepared)
  {
    valid_id_list->intersectList(id_list, id_list_len);
  }
  else
  {
    valid_id_list->reserve(id_list_len);
    for(int i = 0; i < id_list_len; i++)
      valid_id_list->addID(id_list[i]);
  }
}

BasicQueryStrategy Optimizer::ChooseStrategy(BasicQuery *basic_query) {
  return BasicQueryStrategy::Normal;
}

shared_ptr<IntermediateResult> Optimizer::NormalJoin(shared_ptr<BasicQuery>, shared_ptr<QueryPlan>) {
  return shared_ptr<IntermediateResult>();
}

/* This is One layer, need more*/
tuple<bool, TableContentShardPtr> Optimizer::ExecutionDepthFirst(BasicQuery* basic_query,
                                                                 const shared_ptr<QueryPlan>& query_plan,
                                                                 const QueryInfo& query_info,
                                                                 const PositionValueSharedPtr& id_pos_mapping) {
  auto limit_num = query_info.limit_num_;
  cout<<"Optimizer::ExecutionDepthFirst query_info.limit_num_="<<query_info.limit_num_<<endl;
  auto first_result = this->GenerateColdCandidateList((*query_plan->join_order_)[0].edge_filter_->edges_,
                                                      (*query_plan->join_order_)[0].edge_filter_->edges_constant_info_);

  auto var_candidates_cache = make_shared<map<TYPE_ENTITY_LITERAL_ID,shared_ptr<IDList>>>();
  auto first_candidates_list = get<1>(first_result);


  auto first_var_candidates_vec = make_shared<vector<TYPE_ENTITY_LITERAL_ID>>(first_candidates_list->size());
  for(const auto& var_can_vec:*first_candidates_list)
    first_var_candidates_vec->push_back((*var_can_vec)[0]);

  if( query_plan->join_order_->size()==1 || first_candidates_list->empty())
    return make_tuple(true, first_candidates_list);

  int now_result = 0;
  int now_offset = 0;
  vector<TableContentShardPtr> result_container;

  cout<<"ExecutionDepthFirst:"<<endl;

  for(auto& constant_generating_step: *(query_plan->constant_generating_lists_))
  {
    CacheConstantCandidates(constant_generating_step,var_candidates_cache);
  };

  cout<<"first_candidates_list.size()="<<first_candidates_list->size()<<endl;
  /* #TODO 特殊情况：只有一个点，这个还没写
   * */
  if(limit_num != -1) {
    while (now_result <= limit_num) {
      auto tmp_result = make_shared<TableContent>();
      tmp_result->push_back(first_candidates_list->front());
      first_candidates_list->pop_front();
      auto first_var_one_point_result =
          this->DepthSearchOneLayer(query_plan, 1, now_result, limit_num, tmp_result, id_pos_mapping,var_candidates_cache);

      if (get<0>(first_var_one_point_result)) {
        auto one_point_inter_result = get<1>(first_var_one_point_result);
        if (!one_point_inter_result->empty())
          result_container.push_back(one_point_inter_result);
        if (now_result >= limit_num)
          break;
        if (now_offset >= first_var_candidates_vec->size())
          break;
      }
      if (first_candidates_list->empty())
        break;
    }
  }
  else
  {
    while (true) {
      auto tmp_result = make_shared<TableContent>();
      tmp_result->push_back(first_candidates_list->front());
      first_candidates_list->pop_front();
      auto first_var_one_point_result =
          this->DepthSearchOneLayer(query_plan, 1, now_result, limit_num, tmp_result, id_pos_mapping,var_candidates_cache);
      if (get<0>(first_var_one_point_result))
      {
      auto one_point_inter_result = get<1>(first_var_one_point_result);
      if (!one_point_inter_result->empty())
        result_container.push_back(one_point_inter_result);
      if (now_offset >= first_var_candidates_vec->size())
        break;
      }
      if (first_candidates_list->empty())
        break;
    }
  }
  cout<<"Optimizer::ExecutionDepthFirst result_container.size()="<<result_container.size()<<endl;
  if(result_container.empty())
    return make_tuple(true,make_shared<TableContent>());
  int counter = 0;
  /* merge result */

  auto final_result = result_container[0];
  for(int i=1;i<result_container.size();i++)
    for(const auto& record:*(result_container[i])) {
      final_result->push_back(record);
      if( ++counter == limit_num)
        break;
    }

  cout<<"Optimizer::ExecutionDepthFirst final_result.size()="<<final_result->size()<<endl;
  return make_tuple(true,final_result);
}

// #TODO, just return list<shared_ptr<vector< ID_TYPE >>>
tuple<bool,TableContentShardPtr> Optimizer::DepthSearchOneLayer(const shared_ptr<QueryPlan>& query_plan,
                                                                int layer_count,
                                                                int &result_number_till_now,
                                                                int limit_number,
                                                                const TableContentShardPtr& table_content_ptr,
                                                                const PositionValueSharedPtr& id_pos_mapping,
                                                                const IDCachesSharePtr& id_caches) {

  auto one_step = (*(query_plan->join_order_))[layer_count];
  tuple<bool,TableContentShardPtr> step_result;
  switch (one_step.join_type_) {
    case OneStepJoin::JoinType::JoinNode: { // 要注意这里的指针会不会传丢掉
      step_result = JoinANode(one_step.join_node_, table_content_ptr,id_pos_mapping,id_caches);
      break;
    }
    case OneStepJoin::JoinType::JoinTable: {
      break;
    }
    case OneStepJoin::JoinType::GenerateCandidates : {
      // Now Only The first element can be GenerateCandidates
      // We have processed it before
      return make_tuple(false, nullptr);
    }
    case OneStepJoin::JoinType::EdgeCheck: {
      step_result = ANodeEdgesConstraintFilter(one_step.edge_filter_, table_content_ptr,id_pos_mapping,id_caches);
      break;
    }
  }
  // If not success, should have returned before

  auto try_result = get<1>(step_result);

  /* go deeper */
  if(try_result->empty())
    return make_tuple(false, nullptr);


  /* deep in bottom */
  if(layer_count + 1 == query_plan->join_order_->size()) {
    //inter_result->values_->front()->size();
    result_number_till_now += get<1>(step_result)->size();
    return step_result;
  }

  auto all_result = make_shared<TableContent>();



  /* fill a node */
  for(const auto& one_result:*try_result) {
    auto table_content_ptr = make_shared<TableContent>();

    table_content_ptr->push_back(one_result);
    auto next_layer_result  =
        this->DepthSearchOneLayer(query_plan, layer_count + 1, result_number_till_now, limit_number, table_content_ptr,id_pos_mapping,id_caches);
    /* if not success */
    if(!get<0>(next_layer_result))
      continue;

    /* record the result */
    for(const auto& next_layer_one_result: *get<1>(next_layer_result)) {
      all_result->push_back(next_layer_one_result);// all_result->values_->front()->size();
      // cout<<"layer["<<layer_count<<"] record size:"<<next_layer_one_result->size()<<endl;
    }
    if(limit_number!=-1) {
      /* examine the number*/
      if (result_number_till_now >= limit_number)
        return make_tuple(true, all_result);
    }
  }
  return make_tuple(true,all_result);

}


tuple<bool,shared_ptr<IntermediateResult>> Optimizer::DoQuery(SPARQLquery &sparql_query,QueryInfo query_info) {

  cout<<"set query_info = limit 2 begin"<<endl;
  query_info.limit_num_ = 30;
  cout<<"set query_info = limit 2 end"<<endl;
  vector<BasicQuery*> basic_query_vec;
  vector<shared_ptr<QueryPlan>> query_plan_vec;

  for(auto basic_query_pointer:sparql_query.getBasicQueryVec())
  {
    shared_ptr<QueryPlan> query_plan;
    switch (this->ChooseStrategy(basic_query_pointer)) {
      case BasicQueryStrategy::Normal:
        query_plan = make_shared<QueryPlan>(basic_query_pointer,this->kv_store_,this->var_descriptors_);
        break;
      case BasicQueryStrategy::Special:
        printf("BasicQueryStrategy::Special not supported yet\n");
        break;
    }
    cout<<query_plan->toString(kv_store_)<<endl;
    basic_query_vec.push_back(basic_query_pointer);
    query_plan_vec.push_back(query_plan);
    auto mapping_tuple = query_plan->PositionIDMappings();
    auto var_pos_mapping = get<0>(mapping_tuple);
    auto pos_var_mapping = get<1>(mapping_tuple);
    auto basic_query_result = this->ExecutionDepthFirst(basic_query_pointer, query_plan, query_info,var_pos_mapping);
    CopyToResult(basic_query_pointer->getResultListPointer(), basic_query_pointer, make_shared<IntermediateResult>(
        var_pos_mapping,pos_var_mapping,get<1>(basic_query_result)
    ));
  }
  return MergeBasicQuery(sparql_query);
}


tuple<bool, shared_ptr<IntermediateResult>> Optimizer::MergeBasicQuery(SPARQLquery &sparql_query) {
  return tuple<bool, shared_ptr<IntermediateResult>>();
}

/**
 * 调用栈：   Strategy::handle
 *          Strategy::handler0
 *          Join::join_basic
 * @param target
 * @param basic_query
 * @param result, which has already gotten CoreVar, Pre var(selected), but not get satellites
 * @return
 */
bool Optimizer::CopyToResult(vector<unsigned int *> *target,
                             BasicQuery* basic_query,
                             const shared_ptr<IntermediateResult>& result) {

  assert(target->empty());

  int select_var_num = basic_query->getSelectVarNum();
  int core_var_num = basic_query->getRetrievedVarNum();
  int selected_pre_var_num = basic_query->getSelectedPreVarNum();

  // A little different with that in Join::CopyToResult
  // may basic query don't allocate an id for not selected var so selected_pre_var_num = pre_var_num?
  if (result->position_to_var_des_->size() != core_var_num + selected_pre_var_num)
  {
    cout << "terrible error in Optimizer::CopyToResult!" << endl;
    return false;
  }

  shared_ptr<vector<Satellite>> satellites = make_shared<vector<Satellite>>();

  auto record_len = select_var_num + selected_pre_var_num;
  auto record = new unsigned[record_len];

  auto position_id_map_ptr = result->position_to_var_des_;
  auto id_position_map_ptr = result->var_des_to_position_;

  int var_num =  basic_query->getVarNum();
  for (const auto&  record_ptr : *(result->values_))
  {
    int column_index = 0;
    for (; column_index < core_var_num; ++column_index)
    {
      //This is because sleect var id is always smaller
      if ( (*position_id_map_ptr)[column_index] < select_var_num)
      {
        int vpos = basic_query->getSelectedVarPosition((*position_id_map_ptr)[column_index]);
        record[vpos] = (*record_ptr)[column_index];
      }
    }

    // below are for selected pre vars
    while (column_index < result->position_to_var_des_->size() )
    {
      //only add selected ones
      // 原句是 int pre_var_id = this->pos2id[i] - this->var_num;
      // 可能有bug
      int pre_var_id = (*position_id_map_ptr)[column_index] - var_num;
      int pre_var_position = basic_query->getSelectedPreVarPosition(pre_var_id);
      if(pre_var_position >= 0)
      {
        record[pre_var_position] = (*record_ptr)[column_index];
      }
      ++column_index;
    }

    bool valid = true;
    //generate satellites when constructing records
    //NOTICE: satellites in join must be selected
    //core vertex maybe not in select
    for (column_index = 0; column_index < core_var_num; ++column_index)
    {
      int id = (*position_id_map_ptr)[column_index];
      unsigned ele = (*record_ptr)[column_index];
      int degree = basic_query->getVarDegree(id);
      for (int j = 0; j < degree; ++j)
      {
        int id2 = basic_query->getEdgeNeighborID(id, j);
        if (basic_query->isSatelliteInJoin(id2) == false)
          continue;

        unsigned* idlist = nullptr;
        unsigned idlist_len = 0;
        int triple_id = basic_query->getEdgeID(id, j);
        Triple triple = basic_query->getTriple(triple_id);

        TYPE_PREDICATE_ID preid = basic_query->getEdgePreID(id, j);
        if (preid == -2)  //?p
        {
          string predicate = triple.predicate;
          int pre_var_id = basic_query->getPreVarID(predicate);
          //if(this->basic_query->isPreVarSelected(pre_var_id))
          //{
          preid = (*record_ptr)[ (*id_position_map_ptr)[pre_var_id+ var_num]];
          //}
        }
        else if (preid == -1)  //INVALID_PREDICATE_ID
        {
          //ERROR
        }

        char edge_type = basic_query->getEdgeType(id, j);
        if (edge_type == Util::EDGE_OUT)
        {
          this->kv_store_->getobjIDlistBysubIDpreID(ele, preid, idlist, idlist_len, true, this->txn_);
        }
        else
        {
          this->kv_store_->getsubIDlistByobjIDpreID(ele, preid, idlist, idlist_len, true, this->txn_);
        }

        if(idlist_len == 0)
        {
          valid = false;
          break;
        }
        satellites->push_back(Satellite(id2, idlist, idlist_len));

      }
      if(!valid)
      {
        break;
      }
    }

    int size = satellites->size();
    if(valid)
    {
      // Join::cartesian
      Cartesian(0,size,record_len,record,satellites,target,basic_query);
    }

    for (int k = 0; k < size; ++k)
    {
      delete[] (*satellites)[k].idlist;
      //this->satellites[k].idlist = NULL;
    }
    //WARN:use this to avoid influence on the next loop
    satellites->clear();
  }

  delete[] record;


  return false;
}

void
Optimizer::Cartesian(int pos, int end,int record_len,unsigned* record,
                     const shared_ptr<vector<Satellite>>& satellites,
                     vector<unsigned*>* result_list,
                     BasicQuery *basic_query)
{
  if (pos == end)
  {
    auto new_record = new unsigned[record_len];
    memcpy(new_record, record, sizeof(unsigned) * record_len);
    result_list->push_back(new_record);
    return;
  }

  unsigned size = (*satellites)[pos].idlist_len;
  int id = (*satellites)[pos].id;
  int vpos = basic_query->getSelectedVarPosition(id);
  unsigned* list = (*satellites)[pos].idlist;
  for (unsigned i = 0; i < size; ++i)
  {
    record[vpos] = list[i];
    Cartesian(pos + 1, end,record_len,record,satellites,result_list,basic_query);
  }
}




//  Linglin Yang add:


//  Copied from Database::exist_triple in Database.cpp
//  If there exist a triple (s_id, p_id, o_id) in db, return true;
//  else, return false
bool Optimizer::check_exist_this_triple(TYPE_ENTITY_LITERAL_ID s_id, TYPE_PREDICATE_ID p_id, TYPE_ENTITY_LITERAL_ID o_id){

    unsigned* _objidlist = nullptr;
    unsigned _list_len = 0;
    bool is_exist = false;
    //get exclusive before update!
    kv_store_->getobjIDlistBysubIDpreID(s_id, p_id, _objidlist, _list_len, true);

    if (Util::bsearch_int_uporder(o_id, _objidlist, _list_len) != INVALID){
        is_exist = true;
    }
    delete[] _objidlist;

    return is_exist;
}

bool Optimizer::check_past(shared_ptr<BasicQuery> basicquery, vector<int> &join_need_estimator,
                           unsigned *last_sample, unsigned this_var_sample, int this_join_var_num){
    int need_join_id = join_need_estimator[this_join_var_num-1];
    int can_join = true;
    for(int i = 0; i < basicquery->getVarDegree(need_join_id) && can_join; ++i ){
        for(int j = 0; j < this_join_var_num - 1; ++j){
            if(basicquery->getEdgeNeighborID(need_join_id, i) == join_need_estimator[j]){
                char edge_type = basicquery->getEdgeType(need_join_id, i);
                TYPE_PREDICATE_ID p_id = basicquery->getEdgePreID(need_join_id, i);

                if(edge_type == Util::EDGE_IN){
                    if(!check_exist_this_triple(last_sample[j], p_id, this_join_var_num)){
                        can_join = false;
                        break;
                    }
                } else{
                    if(!check_exist_this_triple(this_var_sample, p_id, last_sample[j])){
                        can_join = false;
                        break;
                    }
                }

            }
        }

    }

    return can_join;
}



// TODO： basicquery 中的id?
// TODO: 要做一个从最大传入的接口
unsigned Optimizer::get_small_query_card_estimation(shared_ptr<BasicQuery> basicquery, map<int, unsigned > var_to_num_map,
                                                    map<int, TYPE_ENTITY_LITERAL_ID> var_to_type_map,
                                                    map<int, vector<TYPE_ENTITY_LITERAL_ID >> &var_to_sample_cache,
                                                    vector<int> &join_need_estimator, int this_join_var_num,
                                                    vector<map<vector<int>, unsigned >> &_cardinality_cache,
                                                    vector<map<vector<int>, vector<unsigned *> >> &_sample_cache){

//    _sample_cache begin when var_num >= 2 (var_num =1 cache in var_to_sample_cache)

//    int this_join_var_num = join_need_estimator.size();
    int need_to_join_var_id = join_need_estimator[this_join_var_num-1];
    unsigned card_estimation;
    vector<unsigned *> now_sample;

//    TODO: maybe not used, just read var_to_num_map
    if(this_join_var_num == 1){
//        if(var_to_num_map.find(join_need_estimator[0]) == var_to_num_map.end()){
//            card_estimation = statistics->get_type_num_by_type_id(var_to_type_map[join_need_estimator[0]]);
//            return card_estimation;
//        }
        card_estimation = var_to_num_map[join_need_estimator[0]];

        return card_estimation;
    }

    if(this_join_var_num == 2){
        for(int i = 0; i < basicquery->getVarDegree(join_need_estimator[1]); ++i){
            if(basicquery->getEdgeNeighborID(join_need_estimator[1], i) == join_need_estimator[0]){
                char in_or_out = basicquery->getEdgeType(join_need_estimator[1], i);
                TYPE_PREDICATE_ID p_id = basicquery->getEdgePreID(join_need_estimator[1], i);

                unsigned all_num = statistics->get_type_num_by_type_id(var_to_type_map[join_need_estimator[0]])*
                                   statistics->get_type_num_by_type_id(var_to_type_map[join_need_estimator[1]]);
                double selectivity = var_to_num_map[join_need_estimator[0]]*var_to_num_map[join_need_estimator[1]]/((double )all_num);


                if(in_or_out == Util::EDGE_IN){


                    card_estimation = (unsigned) (statistics->get_type_one_edge_typeid_num_by_id(
                            var_to_type_map[join_need_estimator[0]], p_id, var_to_type_map[join_need_estimator[1]]) * selectivity);

                    int now_sample_num = 0;


                    for(int s_index = 0; s_index < var_to_sample_cache[join_need_estimator[0]].size(); ++s_index){
                        for(int o_index = 0; o_index < var_to_sample_cache[join_need_estimator[1]].size(); ++o_index){
                            if(check_exist_this_triple(var_to_sample_cache[join_need_estimator[0]][s_index],
                                                       p_id, var_to_sample_cache[join_need_estimator[1]][o_index])){
                                now_sample_num += 1;
                                if(now_sample.size()<SAMPLE_CACHE_MAX){

                                    unsigned *this_sample = new unsigned[2];
                                    this_sample[0] = var_to_sample_cache[join_need_estimator[0]][s_index];
                                    this_sample[1] = var_to_sample_cache[join_need_estimator[1]][o_index];

                                    now_sample.push_back(this_sample);
                                } else{
                                    if((double )rand() / (double)RAND_MAX < 1.0/now_sample_num){
                                        int index_to_replace = rand() % SAMPLE_CACHE_MAX;

                                        now_sample[index_to_replace][0] = var_to_sample_cache[join_need_estimator[0]][s_index];
                                        now_sample[index_to_replace][1] = var_to_sample_cache[join_need_estimator[1]][o_index];

                                    }
                                }

                            }
                        }
                    }

                } else{
                    card_estimation = (unsigned) (statistics->get_type_one_edge_typeid_num_by_id(
                            var_to_type_map[join_need_estimator[1]], p_id, var_to_type_map[join_need_estimator[0]]) * selectivity);

                    int now_sample_num = 0;


                    for(int s_index = 0; s_index < var_to_sample_cache[join_need_estimator[1]].size(); ++s_index){
                        for(int o_index = 0; o_index < var_to_sample_cache[join_need_estimator[0]].size(); ++o_index){
                            if(check_exist_this_triple(var_to_sample_cache[join_need_estimator[1]][s_index],
                                                       p_id, var_to_sample_cache[join_need_estimator[0]][o_index])){
                                now_sample_num += 1;
                                if(now_sample.size()<SAMPLE_CACHE_MAX){

                                    unsigned *this_sample = new unsigned[2];
                                    this_sample[0] = var_to_sample_cache[join_need_estimator[0]][o_index];
                                    this_sample[1] = var_to_sample_cache[join_need_estimator[1]][s_index];

                                    now_sample.push_back(this_sample);
                                } else{
                                    if((double )rand() / (double)RAND_MAX < 1.0/now_sample_num){
                                        int index_to_replace = rand() % SAMPLE_CACHE_MAX;

                                        now_sample[index_to_replace][0] = var_to_sample_cache[join_need_estimator[0]][o_index];
                                        now_sample[index_to_replace][1] = var_to_sample_cache[join_need_estimator[1]][s_index];

                                    }
                                }

                            }
                        }
                    }
                }

                if(_cardinality_cache.size() < 1){
                    map<vector<int>, unsigned> this_var_num_card_map;
                    this_var_num_card_map.insert(make_pair(join_need_estimator, card_estimation));
                    _cardinality_cache.push_back(this_var_num_card_map);

                    map<vector<int>, vector<unsigned*>> this_var_num_sample_map;
                    this_var_num_sample_map.insert(make_pair(join_need_estimator, now_sample));
                    _sample_cache.push_back(this_var_num_sample_map);
                } else{
                    _cardinality_cache[0].insert(make_pair(join_need_estimator, card_estimation));
                    _sample_cache[0].insert((make_pair(join_need_estimator, now_sample)));
                }


            }

        }

        return card_estimation;
    }

    if(this_join_var_num >= 3){

        vector<int> last_query(join_need_estimator.begin(), join_need_estimator.end()-1);
//        for(int i = 0; i < this_join_var_num - 1; i++){
//            last_query.push_back(join_need_estimator[i]);
//        }

        unsigned last_card_estimation = _cardinality_cache[this_join_var_num-3][last_query];

        vector<unsigned *> last_sample = _sample_cache[this_join_var_num-3][last_query];
//        vector<unsigned> this_var_sample = var_to_sample_cache[join_need_estimator[this_join_var_num-1]];

        unsigned now_sample_num = 0;

        double smallest_multiplier;
        double selectivity;

        vector<int> edge_link;
        vector<int> node_link;
        vector<TYPE_PREDICATE_ID> p_list;

//        get which node link and edge link to need_to_join_var_id
        for(int i = 0; i < basicquery->getVarDegree(need_to_join_var_id); ++i){
            for(int j = 0; j < this_join_var_num-1; ++j){
                if(basicquery->getEdgeNeighborID(need_to_join_var_id, i) == join_need_estimator[j]){
                    edge_link.push_back(i);
                    node_link.push_back(j);
                    p_list.push_back(basicquery->getEdgePreID(need_to_join_var_id, i));
                }
            }
        }



        TYPE_PREDICATE_ID p1_id = basicquery->getEdgePreID(need_to_join_var_id, edge_link[0]);
        char edge1_type = basicquery->getEdgeType(need_to_join_var_id, edge_link[0]);


        if(edge1_type == Util::EDGE_IN){

            for(int i = 0; i < last_sample.size(); ++i) {

                unsigned *s_o_list = nullptr;
                unsigned s_o_list_len = 0;
                kv_store_->getobjIDlistBysubIDpreID(last_sample[i][node_link[0]], p_list[0], s_o_list, s_o_list_len, true);

                for(int sample_index = 0; sample_index < s_o_list_len; ++sample_index){
                    if(check_exist_this_triple(s_o_list[sample_index], statistics->type_pre_id, var_to_type_map[need_to_join_var_id])){
                        int j;
                        for(j = 1; j < edge_link.size(); ++j){
                            if(basicquery->getEdgeType(need_to_join_var_id, edge_link[j]) == Util::EDGE_IN){
                                if(!check_exist_this_triple(last_sample[i][node_link[j]], p_list[j], s_o_list[sample_index])){
                                    break;
                                }
                            } else{
                                if(!check_exist_this_triple(s_o_list[sample_index], p_list[j], last_sample[i][node_link[j]])){
                                    break;
                                }
                            }
                        }

                        if(j == edge_link.size()){
                            now_sample_num += 1;

                            if(now_sample.size() < SAMPLE_CACHE_MAX){
                                unsigned *need_insert_sample = new unsigned[this_join_var_num];
                                memcpy(need_insert_sample, last_sample[i], sizeof(unsigned )*(this_join_var_num-1));
                                need_insert_sample[this_join_var_num-1] = s_o_list[sample_index];

                                now_sample.push_back(need_insert_sample);
                            } else{
                                if((double )rand() / (double)RAND_MAX < 1.0/now_sample_num){
                                    int index_to_replace = rand()%SAMPLE_CACHE_MAX;

//                                    for(int k = 0; k < this_join_var_num - 1; ++k){
//                                        now_sample[index_to_replace][k] = last_sample[i][k];
//                                    }
                                    memcpy(now_sample[index_to_replace], last_sample[i], this_join_var_num - 1);
                                    now_sample[index_to_replace][this_join_var_num-1] = s_o_list[sample_index];
                                };
                            }
                        }
                    }
                }

                delete[] s_o_list;

            }
            selectivity = (double)now_sample_num/(statistics->get_type_one_edge_typeid_num_by_id(var_to_type_map[node_link[0]],
                                                                                                 p_list[0], var_to_type_map[need_to_join_var_id])
                                                                                                         *last_sample.size());


        } else {

            for (int i = 0; i < last_sample.size(); ++i) {

                unsigned *s_o_list = nullptr;
                unsigned s_o_list_len = 0;
                kv_store_->getsubIDlistByobjIDpreID(last_sample[i][node_link[0]], p_list[0], s_o_list, s_o_list_len,
                                                    true);

                for (int sample_index = 0; sample_index < s_o_list_len; ++sample_index) {
                    if (check_exist_this_triple(s_o_list[sample_index], statistics->type_pre_id,
                                                var_to_type_map[node_link[0]])) {
                        int j;
                        for (j = 1; j < edge_link.size(); ++j) {
                            if (basicquery->getEdgeType(need_to_join_var_id, edge_link[j]) == Util::EDGE_IN) {
                                if (!check_exist_this_triple(last_sample[i][node_link[j]], p_list[j],
                                                             s_o_list[sample_index])) {
                                    break;
                                }
                            } else {
                                if (!check_exist_this_triple(s_o_list[sample_index], p_list[j],
                                                             last_sample[i][node_link[j]])) {
                                    break;
                                }
                            }
                        }

                        if (j == edge_link.size()) {
                            now_sample_num += 1;

                            if (now_sample.size() < SAMPLE_CACHE_MAX) {
                                unsigned *need_insert_sample = new unsigned[this_join_var_num];
                                memcpy(need_insert_sample, last_sample[i], sizeof(unsigned) * (this_join_var_num - 1));
                                need_insert_sample[this_join_var_num - 1] = s_o_list[sample_index];

                                now_sample.push_back(need_insert_sample);
                            } else {
                                if ((double) rand() / (double) RAND_MAX < 1.0 / now_sample_num) {
                                    int index_to_replace = rand() % SAMPLE_CACHE_MAX;

//                                    for(int k = 0; k < this_join_var_num - 1; ++k){
//                                        now_sample[index_to_replace][k] = last_sample[i][k];
//                                    }
                                    memcpy(now_sample[index_to_replace], last_sample[i], this_join_var_num - 1);
                                    now_sample[index_to_replace][this_join_var_num - 1] = s_o_list[sample_index];
                                };
                            }
                        }
                    }
                }

                delete[] s_o_list;



            }
            selectivity = (double)now_sample_num/(statistics->get_type_one_edge_typeid_num_by_id(var_to_type_map[need_to_join_var_id],
                                                                                                 p_list[0], var_to_type_map[node_link[0]])
                                                                                                         *last_sample.size());


        }



        if(now_sample_num == 0){
//            TODO：可以传入一个Int, 如果这种情况赋值 1
            card_estimation = 1;
        } else{
//            double selectivity = (double)now_sample_num/(last_sample.size()*var_to_num_map[join_need_estimator[this_join_var_num-1]]);
            card_estimation = max((unsigned )(last_card_estimation * selectivity), (unsigned )1);

        }

        if(_cardinality_cache.size() < this_join_var_num-1){
            map<vector<int>, unsigned> this_size_card_cache;
            this_size_card_cache.insert(make_pair(join_need_estimator, card_estimation));
            _cardinality_cache.push_back(this_size_card_cache);

            map<vector<int>, vector<unsigned *>> this_size_sample_cache;
            this_size_sample_cache.insert(make_pair(join_need_estimator, now_sample));
            _sample_cache.push_back(this_size_sample_cache);
        } else{
            _cardinality_cache[this_join_var_num-2].insert(make_pair(join_need_estimator, card_estimation));
            _sample_cache[this_join_var_num-2].insert(make_pair(join_need_estimator, now_sample));
        }

        return card_estimation;

    }



}

unsigned Optimizer::cardinality_estimator(shared_ptr<BasicQuery> basicquery, vector<vector<int>> join_need_estimator,
                                          map<int, unsigned> var_to_num_map, map<int, vector<TYPE_ENTITY_LITERAL_ID >> var_to_sample_cache,
                                          map<int, TYPE_ENTITY_LITERAL_ID> var_to_type_map,
                                          vector<map<vector<int>, unsigned >> &_cardinality_cache,
                                          vector<map<vector<int>, vector<unsigned*> >> &_sample_cache) {

    unsigned card_estimation = 0;

    for(auto small_query : join_need_estimator){

        int small_query_var_num = small_query.size();

        if(_cardinality_cache[small_query_var_num-2].find(small_query) != _cardinality_cache[small_query_var_num-2].end()){
            card_estimation += _cardinality_cache[small_query_var_num-2][small_query];
        } else{
            card_estimation += get_small_query_card_estimation(basicquery,var_to_num_map, var_to_type_map, var_to_sample_cache,
                                                               small_query, small_query_var_num, _cardinality_cache, _sample_cache);
        }
    }

    return card_estimation;
}

unsigned Optimizer::card_estimator(shared_ptr<BasicQuery> basicquery, vector<int> last_plan, int this_join_node,
                        vector<map<vector<int>, unsigned>> &_card_cache){
    ;
}


//    save every nei_id of now_in_plan_node in nei_node
void Optimizer::get_nei_by_subplan(shared_ptr<BasicQuery> basicquery, vector<int> &last_plan_node,
                               set<int> &nei_node){
    ;
    for(int node_in_plan : last_plan_node){
        for(int i = 0; i < basicquery->getVarDegree(node_in_plan); ++i){
            if(find(last_plan_node.begin(), last_plan_node.end(), basicquery->getEdgeNeighborID(node_in_plan, i))
                        == last_plan_node.end()){
                nei_node.insert(basicquery->getEdgeNeighborID(node_in_plan, i));
            }
        }
    }
}

void Optimizer::get_last_plan_node(vector<vector<int>> last_plan, vector<int> &last_plan_node){

    for(int i = 1; i < last_plan.size(); ++i){
        for(int node_in_plan : last_plan[i]){
            if(find(last_plan_node.begin(), last_plan_node.end(), node_in_plan) == last_plan_node.end()){
                last_plan_node.push_back(node_in_plan);
            }
        }
    }

    sort(last_plan_node.begin(), last_plan_node.end());
}


//// not complete, maybe not used
//void get_nei_by_subplan(shared_ptr<BasicQuery> basicquery, vector<vector<int>> last_plan,
//                               set<int> &nei_node_ref){
//    ;
////    vector<int>
//    for(auto small_plan : last_plan){
//        for(int node_in_plan : small_plan){
//            for(int i = 0; i < basicquery->getVarDegree(node_in_plan); ++i){
//                if(find(now_in_plan_node.begin(), now_in_plan_node.end(), basicquery->getEdgeNeighborID(node_in_plan, i))
//                   == now_in_plan_node.end()){
//                    nei_node.insert(basicquery->getEdgeNeighborID(node_in_plan, i));
//                }
//            }
//        }
//        for(int i = 0; i < basicquery->getVarDegree(node_in_plan); ++i){
//            if(find(now_in_plan_node.begin(), now_in_plan_node.end(), basicquery->getEdgeNeighborID(node_in_plan, i))
//               == now_in_plan_node.end()){
//                nei_node.insert(basicquery->getEdgeNeighborID(node_in_plan, i));
//            }
//        }
//    }
//}

// first node
void Optimizer::considerallscan(shared_ptr<BasicQuery> basicquery,
                     vector<map<vector<int>, unsigned >> &card_cache_ref,
                     vector<map<vector<vector<int>>, unsigned >> &cost_cache_ref,
                     map<int, unsigned > &var_to_num_map_ref,
                     map<int, TYPE_ENTITY_LITERAL_ID> &var_to_type_map_ref){

    for(int i = 0 ; i < basicquery->getTotalVarNum(); ++i){

        vector<int> this_scan{i};
        vector<vector<int>> this_plan{this_scan};

        for(int j = 0; j < basicquery->getVarDegree(i); ++j){
            if(basicquery->getEdgePreID(i, j) == statistics->type_pre_id){
                var_to_type_map_ref[i] = basicquery->getEdgeNeighborID(i, j);
                var_to_num_map_ref[i] = statistics->get_type_num_by_type_id(basicquery->getEdgeNeighborID(i, j));
            }
        }

        if(card_cache_ref.size() < 1){
            map<vector<int>, unsigned > one_node_card_map;
            map<vector<vector<int>>, unsigned > one_node_cost_map;

            one_node_card_map.insert(make_pair(this_scan, var_to_num_map_ref[i]));
            one_node_cost_map.insert(make_pair(this_plan, var_to_num_map_ref[i]));

            card_cache_ref.push_back(one_node_card_map);
            cost_cache_ref.push_back(one_node_cost_map);
        } else{
            card_cache_ref[0].insert(make_pair(this_scan, var_to_num_map_ref[i]));
            cost_cache_ref[0].insert(make_pair(this_plan, var_to_num_map_ref[i]));
        }
    }

}

// add one node, the added node need to be linked by nodes in plan before
void Optimizer::considerwcojoin(shared_ptr<BasicQuery> basicquery,
                     int node_num, vector<map<vector<int>, unsigned >> &card_cache,
                     vector<map<vector<vector<int>>, unsigned>> &cost_cache){

    for(auto last_plan : cost_cache[node_num - 1]){

        set<int> nei_node;
        vector<int> last_plan_node;

        get_last_plan_node(last_plan.first, last_plan_node);
        get_nei_by_subplan(basicquery, last_plan_node, nei_node);

        for(int next_node : nei_node) {

            if (last_plan.first.size() == 2) {
//            直接接在第一行后面

                vector<vector<int>> new_plan(last_plan.first);
                new_plan[1].push_back(next_node);

                cost_model_for_wco(basicquery, new_plan, next_node,
                                   last_plan_node, last_plan.first,
                                    card_cache, cost_cache);
            } else {
//            需要另起一行

                vector<vector<int>> new_plan(last_plan.first);
                vector<int> next_small_plan{next_node};

                new_plan.push_back(next_small_plan);
                new_plan[0].push_back(new_plan.size()-2);

                cost_model_for_wco(basicquery, new_plan, next_node,
                                   last_plan_node, last_plan.first,
                                   card_cache, cost_cache);
            }
        }
    }

}

void Optimizer::cost_model_for_wco(shared_ptr<BasicQuery> basicquery, vector<vector<int>> &new_plan, int next_node,
                        vector<int> last_plan_node, const vector<vector<int>> &last_plan,
                        vector<map<vector<int>, unsigned>> &card_cache, vector<map<vector<vector<int>>, unsigned>> &cost_cache){
    int last_plan_node_size = last_plan_node.size();

    unsigned wco_cost = cost_cache[last_plan_node_size-1][last_plan];
    wco_cost += card_estimator(basicquery, last_plan_node, next_node, card_cache);


    if(cost_cache.size() <= last_plan_node_size){
        map<vector<vector<int>>, unsigned> new_plan_cost_map;
        new_plan_cost_map.insert(make_pair(new_plan, wco_cost));
        cost_cache.push_back(new_plan_cost_map);
    } else{
        cost_cache[last_plan_node_size].insert(make_pair(new_plan, wco_cost));
    }
}

//  0: cannot do binary join
//  1: small_plan is the small table
//  2: another_small_plan is the small table
int Optimizer::check_two_plan_could_binary_join(vector<int> &last_plan_node,
                                     const vector<vector<int>> &small_plan, const vector<vector<int>> &another_small_plan,
                                     vector<map<vector<int>, unsigned>> &card_cache){

    vector<int> small_plan_node_vec;
    vector<int> another_samll_plan_node_vec;
    get_last_plan_node(small_plan, small_plan_node_vec);
    get_last_plan_node(another_small_plan, another_samll_plan_node_vec);

    bool union_is_not_empty = false;
    int which_table_small = (card_cache[small_plan_node_vec.size()-1][small_plan_node_vec]
                            < card_cache[another_samll_plan_node_vec.size()-1][another_samll_plan_node_vec]) ? 1 : 2;

    for(int j = 0; j < another_samll_plan_node_vec.size(); ++j){
        if(find(small_plan_node_vec.begin(), small_plan_node_vec.end(), another_samll_plan_node_vec[j]) == small_plan_node_vec.end()){
            small_plan_node_vec.push_back(another_samll_plan_node_vec[j]);
        }else{
            union_is_not_empty = true;
        }
    }

    if(union_is_not_empty){

        sort(small_plan_node_vec.begin(), small_plan_node_vec.end());

        if(small_plan_node_vec == last_plan_node){
            return which_table_small;
        } else{
            return 0;
        }
    } else{
        return 0;
    }
}


//  get binary join plan from small_plan and another_samll_plan
//  small_plan is the smaller plan (in the sense of cardinality)
vector<vector<int>> Optimizer::get_binary_plan(const vector<vector<int>> &small_plan, const vector<vector<int>> &another_small_plan){
    vector<vector<int>> binary_plan(small_plan);

    for(int i = 1; i < another_small_plan.size(); ++i){
        binary_plan.push_back(another_small_plan[i]);
    }

    binary_plan[0].push_back(small_plan.size() + another_small_plan.size() - 3);

    for(int i = 0; i < another_small_plan[0].size(); ++i){
        binary_plan[0].push_back(another_small_plan[0][i] + small_plan.size()-2);
    }

    return binary_plan;
}

// not add nodes, but to consider if binaryjoin could decrease cost
void Optimizer::considerbinaryjoin(shared_ptr<BasicQuery> basicquery, int node_num,
                     vector<map<vector<int>, unsigned>> &card_cache,
                     vector<map<vector<vector<int>>, unsigned>> &cost_cache){

    int min_size = 3;
    int max_size = min(min_size, node_num - min_size);


    for(auto need_considerbinaryjoin_plan : cost_cache[node_num - 1]){
        vector<int> last_plan_node;
        get_last_plan_node(need_considerbinaryjoin_plan.first, last_plan_node);

        for(int small_plan_node_num = min_size; small_plan_node_num <= max_size; ++small_plan_node_num){
            for(auto small_plan_cost_pair : cost_cache[small_plan_node_num - 1]){
                for(int another_small_plan_node_num = node_num - small_plan_node_num + 1;
                    another_small_plan_node_num < node_num; ++another_small_plan_node_num){

                    for(auto another_small_plan_cost_pair : cost_cache[another_small_plan_node_num - 1]){

                        int could_binary_join = check_two_plan_could_binary_join(last_plan_node,
                                                                                 small_plan_cost_pair.first, another_small_plan_cost_pair.first, card_cache);
                        if(could_binary_join != 0){

                            if(could_binary_join == 1){
                                unsigned binary_cost = cost_model_for_binary(small_plan_node_num, small_plan_cost_pair.first,
                                                                             another_small_plan_node_num, another_small_plan_cost_pair.first, cost_cache);

                                if(binary_cost < need_considerbinaryjoin_plan.second){
                                    vector<vector<int>> binary_plan = get_binary_plan(small_plan_cost_pair.first, another_small_plan_cost_pair.first);
                                    cost_cache[node_num - 1].insert(make_pair(binary_plan, binary_cost));

                                }
                            } else{
                                unsigned binary_cost = cost_model_for_binary(another_small_plan_node_num, another_small_plan_cost_pair.first,
                                                                             small_plan_node_num, small_plan_cost_pair.first, cost_cache);
                                if(binary_cost < need_considerbinaryjoin_plan.second){
                                    vector<vector<int>> binary_plan = get_binary_plan(another_small_plan_cost_pair.first, small_plan_cost_pair.first);
                                    cost_cache[node_num - 1].insert(make_pair(binary_plan, binary_cost));

                                }
                            }
                        }
                    }
                }
            };
        }
    }
}

//bool check_vector_set_same(set<int> set_need_check, vector<int> vector_need_check){
//    if(set_need_check.size() == vector_need_check.size()){
//        for(int x : vector_need_check){
//            if(set_need_check.find(x) == set_need_check.end()){
//                return false;
//            }
//        }
//    } else{
//        return false;
//    }
//
//    return true;
//}
//
//bool check_vector_set_same(set<int> set_need_check, vector<vector<int>> vector_need_check){
//    set<int> vector_to_set;
//    for(auto x: vector_need_check){
//        for(auto y:x){
//            vector_to_set.insert(y);
//        }
//    }
//
//    if(set_need_check.size() == vector_to_set.size()){
//        for(int x : vector_to_set){
//            if(set_need_check.find(x) == set_need_check.end()){
//                return false;
//            }
//        }
//    } else{
//        return false;
//    }
//    return true;
//
//}

unsigned Optimizer::cost_model_for_binary(int small_plan_node_num, const vector<vector<int>> small_plan,
                               int another_small_plan_node_num, const vector<vector<int>> another_small_plan,
                               vector<map<vector<vector<int>>, unsigned >> &cost_cache){

    return 2*cost_cache[small_plan_node_num][small_plan] + cost_cache[another_small_plan_node_num][another_small_plan];

}

//unsigned Optimizer::cost_model(vector<vector<int>> plan_need_cost,
//                               int var_num, map<int, unsigned > var_to_num_map,
//                               vector<map<vector<int>, unsigned >> &card_cache_ref,
//                               vector<map<vector<vector<int>>, unsigned >> &cost_cache_ref){
//
//    if(cost_cache_ref[var_num - 1].find(plan_need_cost) != cost_cache_ref[var_num - 1].end()){
//        return cost_cache_ref[var_num - 1][plan_need_cost];
//    }
//    unsigned cost = 0;
//    for(int row = 0; row < plan_need_cost.size(); ++row){
//        vector<int> plan;
//        plan.push_back(plan_need_cost[row][0]);
//
////        TODO
//        unsigned row_cost = var_to_num_map[plan_need_cost[row][0]];
//
//        for(int col = 1; col < plan_need_cost[row].size(); ++col){
//            plan.push_back(plan_need_cost[row][col]);
////            TODO
//            row_cost += get_small_query_card_estimation(plan);
//        }
//
//        if(row == 0){
//            cost = row_cost;
//        } else{
//            unsigned small_card = min(row_cost, cost);
//            unsigned large_card = max(row_cost, cost);
//            cost = 2*small_card + large_card;
//        }
//    }
//
//    return cost;
//}


//  enumerate all execution plan, and build cost_cache
void Optimizer::enum_query_plan(shared_ptr<BasicQuery> basicquery, KVstore *kvstore,
                     vector<map<vector<vector<int>>, unsigned>> &cost_cache){

    vector<map<vector<int>, unsigned>> card_cache;

    map<int, unsigned > var_to_num_map;
    map<int, TYPE_ENTITY_LITERAL_ID > var_to_type_map;

    int total_var = basicquery->getTotalVarNum();

    considerallscan(basicquery, card_cache, cost_cache,
                    var_to_num_map, var_to_type_map);

    for(int i = 1; i < total_var; ++i){
        considerwcojoin(basicquery, i+1, card_cache, cost_cache);

        if(i >= 3){
            considerbinaryjoin(basicquery, i+1, card_cache, cost_cache);
        }
    }

}

vector<vector<int>> Optimizer::get_best_plan(int var_num, vector<map<vector<vector<int>>, unsigned>> &cost_cache){

    vector<vector<int>> best_plan;
    unsigned min_cost = UINT_MAX;

    for(auto plan_cost_pair : cost_cache[var_num - 1]){
        if(plan_cost_pair.second < min_cost){
            best_plan = plan_cost_pair.first;
            min_cost = plan_cost_pair.second;
        }
    }

    return best_plan;
}


vector<vector<int>> Optimizer::get_plan(shared_ptr<BasicQuery> basicquery, KVstore *kvstore, IDCachesSharePtr& id_caches){

    vector<map<vector<vector<int>>, unsigned>> cost_cache;

    enum_query_plan(basicquery, kvstore, cost_cache);

    return get_best_plan(basicquery->getVarNum(), cost_cache);
}