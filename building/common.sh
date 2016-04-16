
# print out enviroment variables
env

PWD=`pwd`
echo "PWD:$PWD"
echo "DEPLOY_REPO_SLUG:${DEPLOY_REPO_SLUG}"
ME=` echo "${DEPLOY_REPO_SLUG}/b" | sed 's!\(.*\)\/.*!\1!'`
echo "ME:$ME"

