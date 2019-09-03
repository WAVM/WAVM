#!/bin/bash

# Copied from this repo: https://github.com/k3rn31p4nic/travis-ci-discord-webhook
# Modified by Andrew Scheidecker to support running in both Travis and Azure Pipelines environments

# MIT License
# 
# Copyright (c) 2017 Sankarsan Kampa
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

if [ -z "$2" ]; then
  echo -e "WARNING!!\nYou need to pass the WEBHOOK_URL environment variable as the second argument to this script.\nFor details & guide, visit: https://github.com/k3rn31p4nic/travis-ci-discord-webhook" && exit
fi

if [ "$TRAVIS_COMMIT" ]; then
  COMMIT_ID=$TRAVIS_COMMIT
  BUILD_DESC="Job #$TRAVIS_JOB_NUMBER (Build #$TRAVIS_BUILD_NUMBER)"
  BUILD_URL="https://travis-ci.com/WAVM/WAVM/builds/$TRAVIS_BUILD_ID"
  BRANCH=$BRANCH
elif [ "$BUILD_SOURCEVERSION" ]; then
  COMMIT_ID=$BUILD_SOURCEVERSION
  BUILD_DESC="Build #$BUILD_BUILDID"
  BUILD_URL="https://dev.azure.com/WAVM/WAVM/_build/results?buildId=$BUILD_BUILDID"
  BRANCH=$BUILD_SOURCEBRANCH
else
  echo -e "Couldn't detect build information from environment!"
  exit 1
fi

echo -e "[Webhook]: Sending webhook to Discord...\\n";

case $1 in
  "success" )
    EMBED_COLOR=3066993
    STATUS_MESSAGE="Passed"
    AVATAR="https://travis-ci.com/images/logos/TravisCI-Mascot-blue.png"
    ;;

  "failure" )
    EMBED_COLOR=15158332
    STATUS_MESSAGE="Failed"
    AVATAR="https://travis-ci.com/images/logos/TravisCI-Mascot-red.png"
    ;;

  * )
    EMBED_COLOR=0
    STATUS_MESSAGE="Status Unknown"
    AVATAR="https://travis-ci.com/images/logos/TravisCI-Mascot-1.png"
    ;;
esac

AUTHOR_NAME="$(git log -1 "$COMMIT_ID" --pretty="%aN")"
COMMITTER_NAME="$(git log -1 "$COMMIT_ID" --pretty="%cN")"
COMMIT_SUBJECT="$(git log -1 "$COMMIT_ID" --pretty="%s")"
COMMIT_MESSAGE="$(git log -1 "$COMMIT_ID" --pretty="%b")"

if [ "$AUTHOR_NAME" == "$COMMITTER_NAME" ]; then
  CREDITS="$AUTHOR_NAME authored & committed"
else
  CREDITS="$AUTHOR_NAME authored & $COMMITTER_NAME committed"
fi

URL=""

TIMESTAMP=$(date -u +%FT%TZ)
WEBHOOK_DATA='{
  "username": "",
  "avatar_url": "https://travis-ci.com/images/logos/TravisCI-Mascot-1.png",
  "embeds": [ {
    "color": '$EMBED_COLOR',
    "author": {
      "name": "'"$BUILD_DESC"' '"$STATUS_MESSAGE"' - WAVM/WAVM",
      "url": "'$BUILD_URL'",
      "icon_url": "'$AVATAR'"
    },
    "title": "'"$COMMIT_SUBJECT"'",
    "url": "'"$URL"'",
    "description": "'"${COMMIT_MESSAGE//$'\n'/ }"\\n\\n"$CREDITS"'",
    "fields": [
      {
        "name": "Commit",
        "value": "'"[\`${COMMIT_ID:0:7}\`](https://github.com/WAVM/WAVM/commit/$COMMIT_ID)"'",
        "inline": true
      },
      {
        "name": "Branch/Tag",
        "value": "'"[\`$BRANCH\`](https://github.com/WAVM/WAVM/tree/$BRANCH)"'",
        "inline": true
      }
    ],
    "timestamp": "'"$TIMESTAMP"'"
  } ]
}'

(curl --fail --progress-bar -A "Discord-Webhook" -H Content-Type:application/json -H X-Author:k3rn31p4nic#8383 -d "$WEBHOOK_DATA" "$2" \
  && echo -e "\\n[Webhook]: Successfully sent the webhook.") || echo -e "\\n[Webhook]: Unable to send webhook."
