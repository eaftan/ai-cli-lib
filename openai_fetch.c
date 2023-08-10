#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <readline/history.h>
#include <jansson.h>

#include "config.h"
#include "support.h"

static CURL *curl;
static char *authorization;
static char *system_role;
static const char *program_name;

// Return the response content from an OpenAI JSON response
STATIC char *
get_response_content(const char *json_response)
{
	json_error_t error;
	json_t *root = json_loads(json_response, 0, &error);
	if (!root) {
	    fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
	    return NULL;
	}
	json_t *choices = json_object_get(root, "choices");
	json_t *first_choice = json_array_get(choices, 0);
	json_t *message = json_object_get(first_choice, "message");
	json_t *content = json_object_get(message, "content");

	char *ret = safe_strdup(json_string_value(content));
	json_decref(root);
	return ret;
}

/*
 * Initialize OpenAI connection
 * Return 0 on success -1 on error
 */
int
openai_init(config_t *config)
{
	program_name = short_program_name();
	safe_asprintf(&authorization, "Authorization: Bearer %s", config->openai_key);
	safe_asprintf(&system_role, config->prompt_system, program_name);
	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();
	return curl ? 0 : -1;
}


/*
 * Fetch response from the OpenAI API given the provided prompt.
 * Provide context in the form of n-shot prompts and history prompts.
 */
char *
openai_fetch(config_t *config, const char *prompt, int history_length)
{
	CURLcode res;

	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, authorization);

	struct string json_response;
	string_init(&json_response, "");

	struct string json_request;
	string_init(&json_request, "{\n");
	string_appendf(&json_request, "  \"model\": \"%s\",\n",
	    config->openai_model);
	string_appendf(&json_request, "  \"temperature\": %g,\n",
	    config->openai_temperature);

	string_append(&json_request, "  \"messages\": [\n");
	string_appendf(&json_request,
	    "    {\"role\": \"system\", \"content\": \"%s\"},\n", system_role);


	// Add user and assistant n-shot prompts
	uaprompt_t uaprompts = prompt_find(config, program_name);
	for (int i = 0; uaprompts && i < NPROMPTS; i++) {
		if (uaprompts->user[i])
			string_appendf(&json_request,
			    "    {\"role\": \"user\", \"content\": \"%s\"},\n",
			    uaprompts->user[i]);
		if (uaprompts->assistant[i])
			string_appendf(&json_request,
			    "    {\"role\": \"assistant\", \"content\": \"%s\"},\n",
			    uaprompts->assistant[i]);
	}

	// Add history prompts as context
	for (int i = config->prompt_context - 1; i >= 0; --i) {
		HIST_ENTRY *h = history_get(history_length - 1 - i);
		if (h == NULL)
			continue;
		string_appendf(&json_request,
		    "    {\"role\": \"user\", \"content\": \"%s\"},\n",
		    h->line);
	}

	// Finally, add the user prompt
	string_appendf(&json_request,
	    "    {\"role\": \"user\", \"content\": \"%s\"}\n", prompt);
	string_append(&json_request, "  ]\n}\n");


	// printf("%s", json_request.ptr);
	curl_easy_setopt(curl, CURLOPT_URL, config->openai_endpoint);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_response);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_request.ptr);

	res = curl_easy_perform(curl);

	if (res != CURLE_OK)
	    fprintf(stderr, "OpenAI API call failed: %s\n",
		    curl_easy_strerror(res));

	char *text_response = get_response_content(json_response.ptr);
	free(json_request.ptr);
	free(json_response.ptr);
	return text_response;
}