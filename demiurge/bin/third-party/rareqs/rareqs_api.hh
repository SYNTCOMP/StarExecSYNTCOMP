
#include <list>
#include <vector>
#include <utility>


enum RAReQSQuantifierType
{
  RAReQS_EXISTS = 0,
  RAReQS_FORALL = 1
};

bool RAReQSisSatModel(const std::vector<std::pair<std::vector<int>, RAReQSQuantifierType> > &quantifier_prefix,
                      const std::list<std::vector<int> >& cnf,
                      std::vector<int> &model);