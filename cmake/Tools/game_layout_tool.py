#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#
# CARBONATED
# copy game-specific raw assets from {project_folder}/RawAssets to the target 

import argparse
import logging
import os
import pathlib
import platform
import shutil
import sys
import timeit


def copy_files(source_path: str, target_path: str):
    logging.debug(f'Copy game assets from {source_path} to {target_path}')
    splen = len(source_path)
    for path, dirs, files in os.walk(source_path):
        for file in files:
            source = os.path.join(path, file)
            tail = source[splen + 1:]
            destination = os.path.join(target_path, tail)
            os.makedirs(os.path.basename(destination), exist_ok=True)
            logging.info(f'    copy {source} to {destination}')
            shutil.copyfile(source, destination)


def main(args):
    parser = argparse.ArgumentParser(description='Copy project assets to a layout folder')

    parser.add_argument('--project-path',
                        help='The project path whose assets we will syn.',
                        required=True)
    parser.add_argument('-p', '--platform',
                        help='Target platform for the layout',
                        required=True)
    parser.add_argument('-s', '--source',
                        help='Raw asset source folder',
                        required=True)
    parser.add_argument('-d', '--destination',
                        help='Target layout folder to copy raw assets to',
                        required=True)
    parser.add_argument('--build-config',
                        default='',
                        help='(optional) Build configuration')
    parser.add_argument('--debug',
                        action='store_true',
                        help='Enable debug logs')


    parsed_args = parser.parse_args(args)

    # Prepare the logging
    logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.DEBUG if parsed_args.debug else logging.INFO)

    logging.info(f'Starting raw asset copy from {parsed_args.source} to {parsed_args.destination}')
    start_time = timeit.default_timer()
    copy_files(parsed_args.source, parsed_args.destination)
    duration = timeit.default_timer() - start_time
    logging.info('Asset copy complete {:.2f} seconds'.format(duration))


if __name__ == '__main__':

    try:
        main(sys.argv[1:])
        exit(0)

    except Exception as err:
        print(str(err), file=sys.stderr)
        exit(1)
