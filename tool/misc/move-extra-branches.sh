#!/usr/bin/env bash
set -euo pipefail

# --- CONFIG ---
OLD_REMOTE="origin"
NEW_REMOTE="btstack-extra"
PROTECTED_BRANCHES=("main" "master" "develop" "v0.9")

# --- FUNCTIONS ---
is_protected() {
  local branch="$1"
  for protected in "${PROTECTED_BRANCHES[@]}"; do
    if [[ "$branch" == "$protected" ]]; then
      return 0
    fi
  done
  return 1
}

# --- SCRIPT ---
echo "Fetching latest from $OLD_REMOTE..."
git fetch "$OLD_REMOTE" --prune

# Get all remote branches from OLD_REMOTE
branches=$(git ls-remote --heads "$OLD_REMOTE" | awk '{print $2}' | sed 's|refs/heads/||')

for branch in $branches; do
  if is_protected "$branch"; then
    echo "⚠️ Skipping protected branch: $branch"
    continue
  fi

  echo "➡️ Moving branch: $branch"

  # Push branch to new remote
  # $GIT push "$NEW_REMOTE" "$branch:$branch"
  # Push directly from OLD_REMOTE to NEW_REMOTE without local checkout
  git push "$NEW_REMOTE" "refs/remotes/$OLD_REMOTE/$branch:refs/heads/$branch"

  # If branch exists locally, update upstream to new remote
  if git show-ref --verify --quiet "refs/heads/$branch"; then
    echo "🔗 Updating local branch '$branch' to track $NEW_REMOTE/$branch"
    git branch --set-upstream-to="$NEW_REMOTE/$branch" "$branch"
  fi

  # Delete from OLD_REMOTE
  git push "$OLD_REMOTE" --delete "$branch"
done


echo "✅ Branch migration complete."

echo "Please delete using:"
for branch in $branches; do
  if is_protected "$branch"; then
    continue
  fi
done
