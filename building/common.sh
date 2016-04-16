
# print out enviroment variables
echo "=== Here are all of the environment variables:"
env
echo "==="

PWD=`pwd`
echo "PWD:$PWD"
# This should be populated... oh well
#echo "DEPLOY_REPO_SLUG:${DEPLOY_REPO_SLUG}"
ME=`pwd | sed 's!.*/\(.*\)/VERGE!\1!'`
echo "ME:$ME"

echo "==="
