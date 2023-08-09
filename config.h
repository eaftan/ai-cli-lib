/*-
 *
 *  ai-readline - readline wrapper to obtain a generative AI suggestion
 *  Configuration parsing and access
 *
 *  Copyright 2023 Diomidis Spinellis
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdbool.h>

// Number of supported n-shot prompts
#define NPROMPTS 3

// Linked list for up to three training shots per program
typedef struct uaprompt {
	const char *program;
	const char *user[NPROMPTS];
	const char *assistant[NPROMPTS];
	struct uaprompt *next;
} *uaprompt_t;

typedef struct {
	const char *api_endpoint;
	const char *api_key;
	int prompt_context;		// # past prompts to provide as context
	const char *prompt_system;	// System prompt
	uaprompt_t shots;		// Program-specific training shots
} config_t;

extern bool verbose;

void read_config(config_t *config);
uaprompt_t prompt_find(config_t * config, const char *program_name);