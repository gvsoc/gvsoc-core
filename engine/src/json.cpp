/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 */

#include "vp/json.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <stdio.h>
#include "string.h"
#include <streambuf>
#include <stdlib.h>

std::vector<std::string> split(const std::string& s, char delimiter)
{
   std::vector<std::string> tokens;
   std::string token;
   std::istringstream tokenStream(s);
   while (std::getline(tokenStream, token, delimiter))
   {
      tokens.push_back(token);
   }
   return tokens;
}

js::Config *js::Config::create_config(jsmntok_t *tokens, int *_size)
{
  jsmntok_t *current = tokens;
  Config *config = NULL;

  switch (current->type)
  {
    case JSMN_PRIMITIVE:
      if (strcmp(current->str, "True") == 0 || strcmp(current->str, "False") == 0 ||
        strcmp(current->str, "true") == 0 || strcmp(current->str, "false") == 0)
      {
        config = new ConfigBool(current);
      }
      else
      {
        config = new ConfigNumber(current);
      }
      current++;
      break;

    case JSMN_OBJECT: {
      int size;
      config = new ConfigObject(current, &size);
      current += size;
      break;
    }

    case JSMN_ARRAY: {
      int size;
      config = new ConfigArray(current, &size);
      current += size;
      break;
    }

    case JSMN_STRING:
      config = new ConfigString(current);
      current++;
      break;

    case JSMN_UNDEFINED:
      break;
  }

  if (_size) {
    *_size = current - tokens;
  }

  return config;
}


void js::Config::dump(std::string)
{
}

js::Config *js::ConfigString::get_from_list(std::vector<std::string> name_list)
{
  if (name_list.size() == 0) return this;
  return NULL;
}

void js::ConfigString::dump(std::string)
{
  fprintf(stderr, "\"%s\"", this->value.c_str());
}

js::Config *js::ConfigNumber::get_from_list(std::vector<std::string> name_list)
{
  if (name_list.size() == 0) return this;
  return NULL;
}

void js::ConfigNumber::dump(std::string)
{
  fprintf(stderr, "\"%f\"", this->value);
}

js::Config *js::ConfigArray::get_from_list(std::vector<std::string> name_list)
{
  if (name_list.size() == 0) return this;
  return NULL;
}

void js::ConfigArray::dump(std::string indent)
{
  bool is_first = true;
  fprintf(stderr, "[\n");
  for (auto x: this->elems)
  {
    if (!is_first)
      fprintf(stderr, ",\n");
    x->dump(indent + "  ");
    is_first = false;
  }
  fprintf(stderr, "\n%s]\n", indent.c_str());
}

js::Config *js::ConfigBool::get_from_list(std::vector<std::string> name_list)
{
  if (name_list.size() == 0) return this;
  return NULL;
}

void js::ConfigBool::dump(std::string)
{
  fprintf(stderr, "\"%s\"", this->value ? "true" : "false");
}

void js::ConfigObject::dump(std::string indent)
{
  bool is_first = true;

  fprintf(stderr, "{\n");
  for (auto x: this->childs)
  {
    if (!is_first)
      fprintf(stderr, ",\n");
    fprintf(stderr, "%s  \"%s\": ", indent.c_str(), x.first.c_str());
    x.second->dump(indent + "  ");
    is_first = false;
  }
  fprintf(stderr, "\n%s}\n", indent.c_str());
}

js::Config *js::ConfigObject::get_from_list(std::vector<std::string> name_list)
{
  if (name_list.size() == 0) return this;

  js::Config *result = NULL;
  std::string name;
  int name_pos = 0;

  for (auto& x: name_list) {
    if (x != "*" && x != "**")
    {
      name = x;
      break;
    }
    name_pos++;
  }

  for (auto& x: childs) {

    if (name == x.first)
    {
      result = x.second->get_from_list(std::vector<std::string>(name_list.begin () + name_pos + 1, name_list.begin () + name_list.size()));
      if (name_pos == 0 || result != NULL) return result;

    }
    else if (name_list[0] == "*")
    {
      result = x.second->get_from_list(std::vector<std::string>(name_list.begin () + 1, name_list.begin () + name_list.size()));
      if (result != NULL) return result;
    }
    else if (name_list[0] == "**")
    {
      result = x.second->get_from_list(name_list);
      if (result != NULL) return result;
    }
  }

  return result;
}

js::Config *js::ConfigObject::get(std::string name)
{
  return get_from_list(split(name, '/'));
}

js::ConfigString::ConfigString(jsmntok_t *tokens)
{
  value = std::string(tokens->str);
}

double json_my_stod (std::string const& s) {
    std::istringstream iss (s);
    iss.imbue (std::locale("C"));
    double d;
    iss >> d;
    // insert error checking.
    return d;
}

js::ConfigNumber::ConfigNumber(jsmntok_t *tokens)
{
  value = json_my_stod(tokens->str);
}

js::ConfigBool::ConfigBool(jsmntok_t *tokens)
{
  value = strcmp(tokens->str, "True") == 0 || strcmp(tokens->str, "true") == 0;
}

js::ConfigArray::ConfigArray(jsmntok_t *tokens, int *_size)
{
  jsmntok_t *current = tokens;
  jsmntok_t *top = current++;

  for (int i=0; i<top->size; i++)
  {
    int child_size;
    elems.push_back(create_config(current, &child_size));
    current += child_size;
  }


  if (_size) {
    *_size = current - tokens;
  }
}

js::ConfigObject::ConfigObject(jsmntok_t *tokens, int *_size)
{
  jsmntok_t *current = tokens;
  jsmntok_t *t = current++;

  for (int i=0; i<t->size; i++)
  {
    jsmntok_t *child_name = current++;
    int child_size;
    Config *child_config = create_config(current, &child_size);
    current += child_size;

    if (child_config != NULL)
    {
      childs[child_name->str] = child_config;

    }
  }

  if (_size) {
    *_size = current - tokens;
  }
}

js::Config *js::import_config_from_file(std::string config_path)
{
  std::ifstream t(config_path);
  if(!t.is_open())
  {
      throw std::runtime_error(
              "configuration file does not exist or could not be open");
  }
  std::string str((std::istreambuf_iterator<char>(t)),
                   std::istreambuf_iterator<char>());
  return import_config_from_string(str);

}

js::Config *js::import_config_from_string(std::string config_str)
{
  const char *ConfigString = strdup(config_str.c_str());
  jsmn_parser parser;

  jsmn_init(&parser);
  int nb_tokens = jsmn_parse(&parser, ConfigString, strlen(ConfigString), NULL, 0);

  std::vector<jsmntok_t> tokens(nb_tokens);

  jsmn_init(&parser);
  nb_tokens = jsmn_parse(&parser, ConfigString, strlen(ConfigString), &(tokens[0]), nb_tokens);

  char *str = strdup(ConfigString);
  for (int i=0; i<nb_tokens; i++)
  {
    jsmntok_t *tok = &(tokens[i]);
    tok->str = &str[tok->start];
    str[tok->end] = 0;
      //printf("%d %d %d %d: %s\n", tokens[i].type, tokens[i].start, tokens[i].end, tokens[i].size, tok->str);
  }



  return new js::ConfigObject(&(tokens[0]));
}

int js::ConfigObject::get_child_int(std::string name)
{
  js::Config *config = this->get(name);
  if (config)
    return config->get_int();
  else
    return 0;
}

bool js::ConfigObject::get_child_bool(std::string name)
{
  js::Config *config = this->get(name);
  if (config)
    return config->get_bool();
  else
    return 0;
}

std::string js::ConfigObject::get_child_str(std::string name)
{
  js::Config *config = this->get(name);
  if (config)
    return config->get_str();
  else
    return "";
}
