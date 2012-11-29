//////////////////////////////////////////////////////////////////////////////////////////////
// factory.cc: Factory functions for operators, conditions and condition variables.
//
//
#define UNUSED __attribute__ ((unused))
static char UNUSED rcsId__factory_cc[] = "@(#) $Id$ built on " __DATE__ " " __TIME__;

#include <string>

#include "operators.h"
#include "conditions.h"


///////////////////////////////////////////////////////////////////////////////
// "Factory" functions, processing the parsed lines
//
Operator*
operator_factory(const std::string& op)
{
  Operator* o = NULL;

  if (op == "rm-header") {
    o = new OperatorRMHeader();
  } else if (op == "add-header") {
    o = new OperatorAddHeader();
  } else if (op == "set-status") {
    o = new OperatorSetStatus();
  } else if (op == "set-status-reason") {
    o = new OperatorSetStatusReason();
  } else if (op == "set-destination") {
    o = new OperatorSetDestination();
  } else if (op == "set-redirect") {
    o = new OperatorSetRedirect();
  } else if (op == "timeout-out") {
    o = new OperatorSetTimeoutOut();
  } else if (op == "no-op") {
    o = new OperatorNoOp();
  } else {
    TSError("header_rewrite: unknown operator in header_rewrite: %s", op.c_str());
    return NULL;
  }

  return o;
}


Condition*
condition_factory(const std::string& cond)
{
  Condition* c = NULL;
  std::string c_name, c_qual;
  std::string::size_type pos = cond.find_first_of(':');

  if (pos != std::string::npos) {
    c_name = cond.substr(0, pos);
    c_qual = cond.substr(pos + 1);
  } else {
    c_name = cond;
    c_qual = "";
  }

  if (c_name == "TRUE") {
    c = new ConditionTrue();
  } else if (c_name == "FALSE") {
    c = new ConditionFalse();
  } else if (c_name == "STATUS") {
    c = new ConditionStatus();
  } else if (c_name == "RANDOM") {
    c = new ConditionRandom();
  } else if (c_name == "ACCESS") {
    c = new ConditionAccess();
  } else if (c_name == "HEADER") { // This condition adapts to the hook
    c = new ConditionHeader();
  }else if (c_name == "PATH"){
      c= new ConditionPath();
  } else if (c_name == "CLIENT-HEADER") {
    c = new ConditionHeader(true);
 } else if (c_name == "QUERY") {
     c = new ConditionQuery();
 } else if (c_name == "URL") { // This condition adapts to the hook
    c = new ConditionUrl();
  } else if (c_name == "CLIENT-URL") {
    c = new ConditionUrl(true);
  } else if (c_name == "DBM") {
    c = new ConditionDBM();
  } else {
    TSError("header_rewrite: unknown condition in header_rewrite: %s",c_name.c_str());
    return NULL;
  }

  if (c_qual != "")
    c->set_qualifier(c_qual);

  return c;
}
