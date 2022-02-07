const path = require('path');
const { EOL } = require('os');
const { existsSync } = require('fs');
const { spawn } = require('child_process');

const workspace = process.env.GITHUB_WORKSPACE;
const npmCommand = process.platform === 'win32' ? 'npm.cmd' : 'npm';

(async () => {
  const pkg = getPackageJson();
  const currentVersion = pkg.version.toString();

  console.log('current version:', currentVersion);

  const event = process.env.GITHUB_EVENT_PATH ? require(process.env.GITHUB_EVENT_PATH) : {};

  console.log('release event action:', event.action);

  if (event.action !== 'published' || !event.release) {
    exitSuccess('This action is only run when a release is published.');
    return;
  }

  console.log('release tag name:', event.release.tag_name);

  if (!event.release.tag_name) {
    exitSuccess('This action is only run when a release is published with a tag name.');
    return;
  }

  const releaseTargetBranch = event.release.target_commitish;

  console.log('release target branch:', releaseTargetBranch);

  if (releaseTargetBranch !== 'main') {
    exitSuccess('This action is only run when a release is published from the main branch.');
    return;
  }

  const newVersion = event.release.tag_name.startsWith('v') ? event.release.tag_name.slice(1) : event.release.tag_name;

  console.log('new version', newVersion);

  if (currentVersion === newVersion) {
    exitSuccess('This action is only run when a release is published with a new version.');
    return;
  }

  try {
    console.log('setting git credentials...');

    // Configure the git user for the commit
    await runInWorkspace('git', ['config', 'user.name', `"${process.env.GITHUB_USER || 'CI Version Bump'}"`]);
    await runInWorkspace('git', [
      'config',
      'user.email',
      `"${process.env.GITHUB_EMAIL || 'j.paye96@gmail.com'}"`,
    ]);

    console.log('bumping version with npm...');

    // Bump the version in package.json, and make a commit
    await runInWorkspace(npmCommand, ['version', newVersion]);

    console.log('publishing package...');

    // Publish to npm
    await runInWorkspace(npmCommand, ['publish']);

    console.log('pushing version commit and tag...');

    // Push the package.json commit to the repo
    const remoteRepo = `https://${process.env.GITHUB_ACTOR}:${process.env.GITHUB_TOKEN}@github.com/${process.env.GITHUB_REPOSITORY}.git`;
    await runInWorkspace('git', ['push', remoteRepo]);
  } catch (e) {
    logError(e);
    exitFailure('Failed to bump version');
    return;
  }

  exitSuccess(`Version bumped to v${newVersion} and published!`);
})();

function getPackageJson() {
  const pathToPackage = path.join(workspace, 'package.json');
  if (!existsSync(pathToPackage)) throw new Error("package.json could not be found in your project's root.");
  return require(pathToPackage);
}

function exitSuccess(message) {
  console.info(`✔  success   ${message}`);
  process.exit(0);
}

function exitFailure(message) {
  logError(message);
  process.exit(1);
}

function logError(error) {
  console.error(`✖  fatal     ${error.stack || error}`);
}

function runInWorkspace(command, args) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, { cwd: workspace });

    const errorMessages = [];
    let isDone = false;

    child.on('error', (error) => {
      if (!isDone) {
        isDone = true;
        reject(error);
      }
    });

    child.stderr.on('data', (chunk) => errorMessages.push(chunk));

    child.on('exit', (code) => {
      if (!isDone) {
        if (code === 0) {
          resolve();
        } else {
          reject(`${errorMessages.join('')}${EOL}${command} exited with code ${code}`);
        }
      }
    });
  });
}
