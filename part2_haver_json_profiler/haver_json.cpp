static void PrintToken(AllocOffset * a, Token &t)
{
    switch (t.type) {
    case String:
        fprintf(stdout, "\"%s\"", (char *) OffsetToPtr(a, t.string_offset));
        break;
    case Number:
        fprintf(stdout, "%ld", t.number);
        break;
    case NumberFloat:
        fprintf(stdout, "%lf", t.fraction);
        break;
    case LeftBrace:
        fprintf(stdout, "%c", '{');
        break;
    case RightBrace:
        fprintf(stdout, "%c", '}');
        break;
    case LeftSquare:
        fprintf(stdout, "%c", '[');
        break;
    case RightSquare:
        fprintf(stdout, "%c", ']');
        break;
    case Colon:
        fprintf(stdout, "%c", ':');
        break;
    case Comma:
        fprintf(stdout, "%c", ',');
        break;
    case LiteralTrue:
        fprintf(stdout, "%s", "true");
        break;
    case LiteralFalse:
        fprintf(stdout, "%s", "false");
        break;
    case LiteralNull:
        fprintf(stdout, "%s", "null");
        break;
    default:
        fprintf(stdout, "print of token type not implemented\n");
        break;
    }
}

static void PrintTokenList(AllocOffset *a, Node *head)
{
    u32 curr_offset = PtrToOffset(a, head);
    do {
        Node *curr = (Node *) OffsetToPtr(a, curr_offset);
        Token *token = (Token *) OffsetToPtr(a, curr->token_offset);
        PrintToken(a, *token);
        curr_offset = curr->next_offset;
    }while (curr_offset != 0);
    fprintf(stdout, "\n");
}

static TokenTypes CharToType(char c, FILE *fp)
{
    switch (c) {
        case '{': return LeftBrace;
        case '}': return RightBrace;
        case '[': return LeftSquare;
        case ']': return RightSquare;
        case ',': return Comma;
        case ':': return Colon;
        case 't': {
            char buf[5];
            u32 i = 0;
            buf[i++] = c;
            while (i < 4) {
                buf[i++] = getc(fp);
            }
            buf[i] = '\0';
            return strcmp("true", buf) == 0 ? LiteralTrue : ErrorToken;
        }
        case 'f': {
            char buf[6];
            u32 i = 0;
            buf[i++] = c;
            while (i < 5) {
                buf[i++] = getc(fp);
            }
            buf[i] = '\0';
            return strcmp("false", buf) == 0 ? LiteralFalse : ErrorToken;
        }
        case 'n': {
            char buf[5];
            u32 i = 0;
            buf[i++] = c;
            while (i < 4) {
                buf[i++] = getc(fp);
            }
            buf[i] = '\0';
            return strcmp("null", buf) == 0 ? LiteralNull : ErrorToken;
        }
    }
    fprintf(stderr, "unknown character %c\n", c);
    return ErrorToken;
}

static u32 ExtractTokens(FILE *fp, AllocOffset *a)
{
    assert(fp != NULL);

    u32 curr_offset = PtrToOffset(a, AOAlloc(a, sizeof(Node)));
    u32 head_offset = curr_offset;

    char c;
    bool number_found = false;
    while (number_found || ((c = getc(fp)) != EOF)) {
        number_found = false;

        while (strchr(" \n\r\t", c)) c = getc(fp);

        if (c == '"') {
            // u8 *buf = AOAlloc(a, 129);
            // u32 buf_offset = PtrToOffset(a, buf);
            u8 buf[129];

            u32 i = 0;
            char prev = c;
            c = getc(fp);
            while ((c != EOF) && (!((prev != '\\') && (c == '"')))) {
                if (i > 128) {
                    fprintf(stderr, "string \"%s\" is too long. String cannot be longer than 128 char\n", buf);
                    goto end_extract;
                }
                buf[i++] = c;
                prev = c;
                c = getc(fp);
            }
            buf[i++] = '\0';
            u8 *heap_buf = AOAlloc(a, i);
            memcpy(heap_buf, buf, i);
            u32 buf_offset = PtrToOffset(a, heap_buf);

            Token *token = (Token *) AOAlloc(a, sizeof(Token));
            *token = {.type = String, .string_offset = buf_offset};
            u32 token_offset = PtrToOffset(a, token);

            Node *nn = (Node *) AOAlloc(a, sizeof(Node));
            u32 nn_offset = PtrToOffset(a, nn);
            nn->token_offset = token_offset;
            nn->prev_offset = curr_offset;
            ((Node *)OffsetToPtr(a, curr_offset))->next_offset = PtrToOffset(a, nn);
            curr_offset = nn_offset;

        } else if (strchr("-0123456789",c)) {
            // u8 *buf = AOAlloc(a, 64);
            // u32 buf_offset = PtrToOffset(a, buf);
            char buf[65];
            buf[0] = c;
            c = getc(fp);

            bool found_frac = false;
            bool found_e = false;
            bool second_minus = false;
            u32 i = 1;
            {
            while ((c != EOF) && (('0' <= c && c <= '9') || c == '-' || c == '.' || c == 'e')) {
                if (i > 64) {
                    fprintf(stderr, "number \"%s\" is too long. Number cannot be longer than 64 digits\n", buf);
                    goto end_extract;
                }
                if (c == '.') {
                    if (!found_frac) {
                        found_frac = true;
                    } else {
                        fprintf(stderr, "number \"%s\" has second decimal!\n", buf);
                        goto end_extract;
                    }
                } else if (c == 'e' || c == 'E') {
                    if (!found_e) {
                        found_e = true;
                    } else {
                        fprintf(stderr, "number \"%s\" has second e/E!\n", buf);
                        goto end_extract;
                    }
                } else if (c == '-') {
                    if (second_minus) {
                        fprintf(stderr, "number \"%s\" has third minus sign!!\n", buf);
                        goto end_extract;
                    } else if (!found_e) {
                        fprintf(stderr, "number \"%s\" has second minus sign!!\n", buf);
                        goto end_extract;
                    }
                    second_minus = true;
                }

                buf[i++] = c;
                c = getc(fp);
            }
            }
            buf[i] = '\0';
            number_found = true;

            Token *token = (Token *) AOAlloc(a, sizeof(Token));
            u32 token_offset = PtrToOffset(a, token);
            if (found_frac || found_e) {
                *token = {.type = NumberFloat, .fraction = atof(buf)};
            } else {
                *token = {.type = Number, .number = atol(buf)};
            }

            Node *nn = (Node *) AOAlloc(a, sizeof(Node));
            u32 nn_offset = PtrToOffset(a, nn);
            nn->token_offset = token_offset;
            nn->prev_offset = curr_offset;
            ((Node *)OffsetToPtr(a, curr_offset))->next_offset = PtrToOffset(a, nn);
            curr_offset = nn_offset;

        } else if (strchr("{}[]:,tfn",c)) {
            Token *token = (Token *) AOAlloc(a, sizeof(Token));
            *token = {.type = CharToType(c, fp)};
            if (token->type == ErrorToken) {
                goto end_extract;
            }
            u32 token_offset = PtrToOffset(a, token);

            Node *nn = (Node *) AOAlloc(a, sizeof(Node));
            u32 nn_offset = PtrToOffset(a, nn);
            nn->token_offset = token_offset;
            nn->prev_offset = curr_offset;
            ((Node *)OffsetToPtr(a, curr_offset))->next_offset = PtrToOffset(a, nn);
            curr_offset = nn_offset;
        }

    }

end_extract:
    // remove dummy head token
    // *head_token = *head_token->next->next;
    Node *node = (Node *) OffsetToPtr(a, head_offset);
    return node->next_offset;
}

static void ParseValue(AllocOffset *a, u32 value_offset, u32 *node_offset)
{
    assert(a != NULL && node_offset != NULL);

    Node *node = (Node *) OffsetToPtr(a, *node_offset);
    Token *token = (Token *) OffsetToPtr(a, node->token_offset);
    Value *value = (Value *) OffsetToPtr(a, value_offset);
    u32 object_offset;
    u32 array_offset;
    switch (token->type) {
        case String:
            value->type = ValueString;
            value->string_offset = token->string_offset;
            break;
        case Number:
            value->type = ValueNumber;
            value->number = token->number;
            break;
        case NumberFloat:
            value->type = ValueFraction;
            value->fraction = token->fraction;
            break;
        case LiteralTrue:
            value->type = ValueTrue;
            break;
        case LiteralFalse:
            value->type = ValueFalse;
            break;
        case LiteralNull:
            value->type = ValueNull;
            break;
        case LeftBrace:
            value->type = ValueObject;
            object_offset = PtrToOffset(a, AOAlloc(a, sizeof(Object)));
            ((Value *)OffsetToPtr(a, value_offset))->object_offset = object_offset;
            ParseObject(a, object_offset, node_offset);
            break;
        case LeftSquare:
            value->type = ValueArray;
            array_offset = PtrToOffset(a, AOAlloc(a, sizeof(Array)));
            // NOTE: This is just array_offset but Parse..Sep is drifing from Parse..
            ((Value *)OffsetToPtr(a, value_offset))->array_head_offset = array_offset;
            ParseArray(a, array_offset, node_offset);
            break;

        case RightBrace:
        case RightSquare:
        case Colon:
        case Comma:
        case ErrorToken:
        case TokenCount:
            fprintf(stderr, "incorrect token type for value\n");
            exit(EXIT_FAILURE);
            break;
    }
}

static void ParseObject(AllocOffset *a, u32 object_offset, u32 *node_offset)
{
    assert(a != NULL && node_offset != NULL);

    Node *node = (Node *) OffsetToPtr(a, *node_offset);
    Token *token = (Token *) OffsetToPtr(a, node->token_offset);
    if (token->type != LeftBrace) {
        fprintf(stderr, "object needs to start with '{'\n");
        exit(EXIT_FAILURE);
    }
    *node_offset = node->next_offset;
    node = (Node *) OffsetToPtr(a, *node_offset);
    token = (Token *) OffsetToPtr(a, node->token_offset);

    if ( *node_offset != 0 && (token->type != String && token->type != RightBrace)) {
        fprintf(stderr, "should either be empty object or object key should be of type string\n");
        exit(EXIT_FAILURE);
    }

    // NOTE: it's not really comma, but left brace BUT helps with switch statement state machine
    TokenTypes prev_type = Comma;
    bool is_first;
    u32 curr_obj_offset = object_offset;
    Object *curr_obj;

    u32 key_offset;
    u32 new_obj_offset;
    Object *new_obj;
    u32 item_offset;
    ObjectItem *item;
    u32 value_offset;
    while ((*node_offset != 0)) {
        node = (Node *) OffsetToPtr(a, *node_offset);
        token = (Token *) OffsetToPtr(a, node->token_offset);
        switch (token->type) {
        case String:
            if (prev_type != Comma) {
                fprintf(stdout, "object item must be either after a comma or at start of object\n");
                exit(EXIT_FAILURE);
            }

            key_offset = token->string_offset;
            *node_offset = node->next_offset;
            node = (Node *) OffsetToPtr(a, *node_offset);
            token = (Token *) OffsetToPtr(a, node->token_offset);

            if (*node_offset == 0 || token->type != Colon) {
                fprintf(stdout, "token after object key must COLON\n");
                exit(EXIT_FAILURE);
            }
            *node_offset = node->next_offset;
            node = (Node *) OffsetToPtr(a, *node_offset);
            token = (Token *) OffsetToPtr(a, node->token_offset);

            if (*node_offset == 0) {
                fprintf(stdout, "object must have value\n");
                exit(EXIT_FAILURE);
            }

            item_offset = PtrToOffset(a, (ObjectItem *) AOAlloc(a, sizeof(ObjectItem)));
            curr_obj = (Object *) OffsetToPtr(a, curr_obj_offset);
            curr_obj->item_offset = item_offset;

            value_offset = PtrToOffset(a, (Value *) AOAlloc(a, sizeof(Value)));
            item = (ObjectItem *) OffsetToPtr(a, item_offset);
            *item = {.key_offset = key_offset, .value_offset = value_offset};

            ParseValue(a, value_offset, node_offset);

            // NOTE: it's not really string but val, BUT helps with switch statement state machine
            prev_type = String;
            break;
        case Comma:
            if (prev_type != String) {
                fprintf(stderr, "comma must follow object item i.e. key value pair\n");
                exit(EXIT_FAILURE);
            }

            new_obj_offset = PtrToOffset(a, AOAlloc(a, sizeof(Object)));
            new_obj = (Object *) OffsetToPtr(a, new_obj_offset);
            curr_obj = (Object *) OffsetToPtr(a, curr_obj_offset);

            new_obj->prev_offset = curr_obj_offset;
            curr_obj->next_offset = new_obj_offset;
            curr_obj_offset = new_obj_offset;

            prev_type = Comma;
            break;
        case RightBrace:
            if (prev_type != String) {
                fprintf(stderr, "cannot have trailing comma in object\n");
                exit(EXIT_FAILURE);
            }
            return;

        case ErrorToken:
        case LeftBrace:
        case LeftSquare:
        case RightSquare:
        case Colon:
        case LiteralTrue:
        case LiteralFalse:
        case LiteralNull:
        case Number:
        case NumberFloat:
        case TokenCount:
            fprintf(stderr, "incorrect array type\n");
            exit(EXIT_FAILURE);
            break;
        }

        node = (Node *) OffsetToPtr(a, *node_offset);
        *node_offset = node->next_offset;
    }
}

static void ParseArray(AllocOffset *a, u32 array_offset, u32 *node_offset)
{
    assert(a != NULL && node_offset != NULL);

    Node *node = (Node *) OffsetToPtr(a, *node_offset);
    Token *token = (Token *) OffsetToPtr(a, node->token_offset);
    if (token->type != LeftSquare) {
        fprintf(stderr, "array needs to start with '['\n");
        exit(EXIT_FAILURE);
    }
    *node_offset = node->next_offset;
    node = (Node *) OffsetToPtr(a, *node_offset);
    token = (Token *) OffsetToPtr(a, node->token_offset);

    u32 curr_offset = array_offset;
    Array *curr;
    u32 value_offset;
    Value *value;
    u32 new_array_offset;
    Array *new_array;

    // NOTE: it's not really comma, but LeftSquare BUT helps with switch statement state machine
    TokenTypes prev_type = Comma;
    while (*node_offset != 0) {
        node = (Node *) OffsetToPtr(a, *node_offset);
        token = (Token *) OffsetToPtr(a, node->token_offset);

        switch (token->type) {
            case LeftBrace:
            case LeftSquare:
            case LiteralTrue:
            case LiteralFalse:
            case LiteralNull:
            case Number:
            case NumberFloat:
            case String:
                if (prev_type != Comma) {
                    fprintf(stderr, "prev type for array needs to be , or [, %s\n", (char *) OffsetToPtr(a, token->string_offset));
                    exit(EXIT_FAILURE);
                }

                value_offset = PtrToOffset(a,AOAlloc(a, sizeof(Value)));
                curr = (Array *) OffsetToPtr(a, curr_offset);
                curr->value_offset = value_offset;

                ParseValue(a, value_offset, node_offset);

                // NOTE: it's not really string, but val BUT helps with switch statement state machine
                prev_type = String;
                break;

            case Comma:
                if (prev_type != String) {
                    fprintf(stderr, "prev type for array needs to be value type [\n");
                    exit(EXIT_FAILURE);
                }

                new_array_offset = PtrToOffset(a, AOAlloc(a, sizeof(Array)));
                new_array = (Array *) OffsetToPtr(a, new_array_offset);
                curr = (Array *) OffsetToPtr(a, curr_offset);

                new_array->prev_offset = curr_offset;
                curr->next_offset = new_array_offset;
                curr_offset = new_array_offset;

                prev_type = Comma;
                break;

            case RightSquare:
                if (prev_type != String) {
                    fprintf(stderr, "array cannot have trailing ,\n");
                    exit(EXIT_FAILURE);
                }
                return;

            case TokenCount:
            case ErrorToken:
            case Colon:
            case RightBrace:
                fprintf(stderr, "comma must follow object item i.e. key value pair\n");
                exit(EXIT_FAILURE);
                break;
        }
        node = (Node *) OffsetToPtr(a, *node_offset);
        *node_offset = node->next_offset;
    }
}

static void ParseValueSep(AllocOffset *a_json, u32 value_offset, AllocOffset *a_token, u32 *node_offset)
{
    // TimeFunction;
    assert(a_json != NULL && a_token != NULL && node_offset != NULL);

    Node *node = (Node *) OffsetToPtr(a_token, *node_offset);
    Token *token = (Token *) OffsetToPtr(a_token, node->token_offset);
    Value *value = (Value *) OffsetToPtr(a_json, value_offset);
    u32 object_offset;
    u32 array_head_offset;
    char *s;
    char *js;
    switch (token->type) {
        case String:
            value->type = ValueString;
            s = (char *) OffsetToPtr(a_token, token->string_offset);
            // NOTE: len + 1 for null terminator
            js = (char *) AOAlloc(a_json, strlen(s)+1);
            memcpy(js, s, strlen(s)+1);
            value->string_offset = PtrToOffset(a_json, js);

            break;
        case Number:
            value->type = ValueNumber;
            value->number = token->number;
            break;
        case NumberFloat:
            value->type = ValueFraction;
            value->fraction = token->fraction;
            break;
        case LiteralTrue:
            value->type = ValueTrue;
            break;
        case LiteralFalse:
            value->type = ValueFalse;
            break;
        case LiteralNull:
            value->type = ValueNull;
            break;
        case LeftBrace:
            value->type = ValueObject;
            object_offset = PtrToOffset(a_json, AOAlloc(a_json, sizeof(Object)));
            ((Value *)OffsetToPtr(a_json, value_offset))->object_offset = object_offset;
            ParseObjectSep(a_json, object_offset, a_token, node_offset);
            break;
        case LeftSquare:
            value->type = ValueArray;
            array_head_offset = PtrToOffset(a_json, AOAlloc(a_json, sizeof(ArrayHead)));
            ((Value *)OffsetToPtr(a_json, value_offset))->array_head_offset = array_head_offset;
            ParseArraySep(a_json, array_head_offset, a_token, node_offset);
            break;

        case RightBrace:
        case RightSquare:
        case Colon:
        case Comma:
        case ErrorToken:
        case TokenCount:
            fprintf(stderr, "incorrect token type for value\n");
            exit(EXIT_FAILURE);
            break;
    }

}

static void ParseObjectSep(AllocOffset *a_json, u32 object_offset, AllocOffset *a_token, u32 *node_offset)
{
    // TimeFunction;
    assert(a_json != NULL && a_token != NULL && node_offset != NULL);

    Node *node = (Node *) OffsetToPtr(a_token, *node_offset);
    Token *token = (Token *) OffsetToPtr(a_token, node->token_offset);
    if (token->type != LeftBrace) {
        fprintf(stderr, "object needs to start with '{'\n");
        exit(EXIT_FAILURE);
    }
    *node_offset = node->next_offset;
    node = (Node *) OffsetToPtr(a_token, *node_offset);
    token = (Token *) OffsetToPtr(a_token, node->token_offset);

    if ( *node_offset != 0 && (token->type != String && token->type != RightBrace)) {
        fprintf(stderr, "should either be empty object or object key should be of type string\n");
        exit(EXIT_FAILURE);
    }

    // NOTE: it's not really comma, but left brace BUT helps with switch statement state machine
    TokenTypes prev_type = Comma;
    u32 curr_obj_offset = object_offset;
    Object *curr_obj;

    u32 key_offset;
    u32 new_obj_offset;
    Object *new_obj;
    u32 item_offset;
    ObjectItem *item;
    u32 value_offset;
    char *s;
    char *js;
    while ((*node_offset != 0)) {
        node = (Node *) OffsetToPtr(a_token, *node_offset);
        token = (Token *) OffsetToPtr(a_token, node->token_offset);
        switch (token->type) {
        case String:
            if (prev_type != Comma) {
                fprintf(stdout, "object item must be either after a comma or at start of object\n");
                exit(EXIT_FAILURE);
            }

            s = (char *) OffsetToPtr(a_token, token->string_offset);
            // NOTE: len + 1 for null terminator
            js = (char *) AOAlloc(a_json, strlen(s)+1);
            memcpy(js, s, strlen(s)+1);

            key_offset = PtrToOffset(a_json, js);
            *node_offset = node->next_offset;
            node = (Node *) OffsetToPtr(a_token, *node_offset);
            token = (Token *) OffsetToPtr(a_token, node->token_offset);

            if (*node_offset == 0 || token->type != Colon) {
                fprintf(stdout, "token after object key must COLON\n");
                exit(EXIT_FAILURE);
            }
            *node_offset = node->next_offset;
            node = (Node *) OffsetToPtr(a_token, *node_offset);
            token = (Token *) OffsetToPtr(a_token, node->token_offset);

            if (*node_offset == 0) {
                fprintf(stdout, "object must have value\n");
                exit(EXIT_FAILURE);
            }

            item_offset = PtrToOffset(a_json, (ObjectItem *) AOAlloc(a_json, sizeof(ObjectItem)));
            curr_obj = (Object *) OffsetToPtr(a_json, curr_obj_offset);
            curr_obj->item_offset = item_offset;

            value_offset = PtrToOffset(a_json, (Value *) AOAlloc(a_json, sizeof(Value)));
            item = (ObjectItem *) OffsetToPtr(a_json, item_offset);
            *item = {.key_offset = key_offset, .value_offset = value_offset};

            ParseValueSep(a_json, value_offset, a_token, node_offset);

            // NOTE: it's not really string but val, BUT helps with switch statement state machine
            prev_type = String;
            break;
        case Comma:
            if (prev_type != String) {
                fprintf(stderr, "comma must follow object item i.e. key value pair\n");
                exit(EXIT_FAILURE);
            }

            new_obj_offset = PtrToOffset(a_json, AOAlloc(a_json, sizeof(Object)));
            new_obj = (Object *) OffsetToPtr(a_json, new_obj_offset);
            curr_obj = (Object *) OffsetToPtr(a_json, curr_obj_offset);

            new_obj->prev_offset = curr_obj_offset;
            curr_obj->next_offset = new_obj_offset;
            curr_obj_offset = new_obj_offset;

            prev_type = Comma;
            break;
        case RightBrace:
            if (prev_type != String) {
                fprintf(stderr, "cannot have trailing comma in array\n");
                exit(EXIT_FAILURE);
            }
            return;

        case ErrorToken:
        case LeftBrace:
        case LeftSquare:
        case RightSquare:
        case Colon:
        case LiteralTrue:
        case LiteralFalse:
        case LiteralNull:
        case Number:
        case NumberFloat:
        case TokenCount:
            fprintf(stderr, "incorrect array type\n");
            exit(EXIT_FAILURE);
            break;
        }

        node = (Node *) OffsetToPtr(a_token, *node_offset);
        *node_offset = node->next_offset;
    }
}

static void ParseArraySep(AllocOffset *a_json, u32 array_head_offset, AllocOffset *a_token, u32 *node_offset)
{
    assert(a_json != NULL && a_token != NULL && node_offset != NULL);

    Node *node = (Node *) OffsetToPtr(a_token, *node_offset);
    Token *token = (Token *) OffsetToPtr(a_token, node->token_offset);
    if (token->type != LeftSquare) {
        fprintf(stderr, "array needs to start with '['\n");
        exit(EXIT_FAILURE);
    }
    *node_offset = node->next_offset;
    // TODO: are these two definitions even necessary?
    node = (Node *) OffsetToPtr(a_token, *node_offset);
    token = (Token *) OffsetToPtr(a_token, node->token_offset);

    ArrayHead *ah = (ArrayHead *) OffsetToPtr(a_json, array_head_offset);
    u32 array_offset = PtrToOffset(a_json, AOAlloc(a_json, sizeof(Array)));
    ah->array_offset = array_offset;

    u64 size = 0;
    u32 curr_offset = array_offset;
    Array *curr;
    u32 value_offset;
    Value *value;
    u32 new_array_offset;
    Array *new_array;

    // NOTE: it's not really comma, but LeftSquare BUT helps with switch statement state machine
    TokenTypes prev_type = Comma;
    while (*node_offset != 0) {
        node = (Node *) OffsetToPtr(a_token, *node_offset);
        token = (Token *) OffsetToPtr(a_token, node->token_offset);

        switch (token->type) {
            case LeftBrace:
            case LeftSquare:
            case LiteralTrue:
            case LiteralFalse:
            case LiteralNull:
            case Number:
            case NumberFloat:
            case String:
                if (prev_type != Comma) {
                    fprintf(stderr, "prev type for array needs to be , or [, %s\n", (char *) OffsetToPtr(a_token, token->string_offset));
                    exit(EXIT_FAILURE);
                }

                value_offset = PtrToOffset(a_json,AOAlloc(a_json, sizeof(Value)));
                curr = (Array *) OffsetToPtr(a_json, curr_offset);
                curr->value_offset = value_offset;
                size ++;

                ParseValueSep(a_json, value_offset, a_token, node_offset);

                // NOTE: it's not really string, but val BUT helps with switch statement state machine
                prev_type = String;
                break;

            case Comma:
                if (prev_type != String) {
                    fprintf(stderr, "prev type for array needs to be value type [\n");
                    exit(EXIT_FAILURE);
                }

                new_array_offset = PtrToOffset(a_json, AOAlloc(a_json, sizeof(Array)));
                new_array = (Array *) OffsetToPtr(a_json, new_array_offset);
                curr = (Array *) OffsetToPtr(a_json, curr_offset);

                new_array->prev_offset = curr_offset;
                curr->next_offset = new_array_offset;
                curr_offset = new_array_offset;

                prev_type = Comma;
                break;

            case RightSquare:
                if (prev_type != String) {
                    fprintf(stderr, "array cannot have trailing ,\n");
                    exit(EXIT_FAILURE);
                }
                ah = (ArrayHead *) OffsetToPtr(a_json, array_head_offset);
                ah->size = size;
                return;

            case TokenCount:
            case ErrorToken:
            case Colon:
            case RightBrace:
                fprintf(stderr, "comma must follow object item i.e. key value pair\n");
                exit(EXIT_FAILURE);
                break;
        }
        node = (Node *) OffsetToPtr(a_token, *node_offset);
        *node_offset = node->next_offset;
    }
}

// NOTE: the value 0 means that it is not found
static u32 FindValue(Json *json, char *target_key)
{
    u32 curr_off = json->head_offset;
    Object *object = (Object *) OffsetToPtr(&json->a, curr_off);
    ObjectItem *item = (ObjectItem *) OffsetToPtr(&json->a, object->item_offset);
    char *curr_key = (char *) OffsetToPtr(&json->a, item->key_offset);
    do {
        if (target_key[0] == curr_key[0] && (strcmp(target_key, curr_key) == 0)) {
            return item->value_offset;
        }
        curr_off = object->next_offset;
        object = (Object *) OffsetToPtr(&json->a, curr_off);
        item = (ObjectItem *) OffsetToPtr(&json->a, object->item_offset);
        curr_key = (char *) OffsetToPtr(&json->a, item->key_offset);
    } while (curr_off != 0);

    return 0;
}

static u32 GetArrayHead(Json *json, u32 value_off)
{
    Value *v = (Value *) OffsetToPtr(&json->a, value_off);
    assert(v->type == ValueArray);
    return v->array_head_offset;
}

static void PrintValue(FILE *dst, AllocOffset *a, u32 value_offset)
{
    Value *val = (Value *) OffsetToPtr(a, value_offset);
    switch (val->type) {
        case ValueNumber:
            fprintf(dst, "%ld", val->number);
            break;
        case ValueString:
            fprintf(dst, "\"%s\"", (char *) OffsetToPtr(a, val->string_offset));
            break;
        case ValueFraction:
            fprintf(dst, "%lf", val->fraction);
            break;
        case ValueTrue:
            fprintf(dst, "true");
            break;
        case ValueFalse:
            fprintf(dst, "false");
            break;
        case ValueNull:
            fprintf(dst, "null");
            break;
        case ValueArray:
            PrintArray(dst, a, val->array_head_offset);
            break;
        case ValueObject:
            PrintObject(dst, a, val->object_offset);
            break;
        case ValueNone:
        case ValueCount:
            fprintf(dst, "unknown value type\n");
            break;
    }
}

static void PrintObject(FILE *dst, AllocOffset *a, u32 object_offset)
{
    u32 curr_offset = object_offset;
    Object *curr;
    ObjectItem *item;

    fprintf(dst, "{");
    do {
        curr = (Object *) OffsetToPtr(a, curr_offset);
        item = (ObjectItem *) OffsetToPtr(a, curr->item_offset);

        fprintf(dst, "\"%s\":", (char *) OffsetToPtr(a, item->key_offset));
        PrintValue(dst, a, item->value_offset);

        curr_offset = curr->next_offset;
        if (curr_offset == 0) {
            fprintf(dst, "}");
        }else {
            fprintf(dst, ",");
        }
    } while (curr_offset != 0);
}

static void PrintArray(FILE *dst, AllocOffset *a, u32 array_head_offset)
{
    ArrayHead *ah = (ArrayHead *) OffsetToPtr(a, array_head_offset);
    u32 curr_offset = ah->array_offset;
    assert(curr_offset != 0);

    Array *curr;

    fprintf(dst, "[");
    while (curr_offset != 0) {
        curr = (Array *) OffsetToPtr(a, curr_offset);

        PrintValue(dst, a, curr->value_offset);

        curr_offset = curr->next_offset;
        if (curr_offset == 0) {
            fprintf(dst, "]");
        }else {
            fprintf(dst, ",");
        }
    }
}

static void PrintJson(FILE *dst, Json *json)
{
    PrintObject(dst, &json->a, json->head_offset);
}

static u32 ParseJson(AllocOffset *allocator, char *filename)
{
    AOInit(allocator, 4096);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "cannot open file: %s, for reading\n", filename);
    }

    fprintf(stdout, "%s: memory size\n", filename);
    fprintf(stdout, "run,sizeB,capacityB,sizekB,capacitykB\n");
    u32 head_token_offset = ExtractTokens(fp, allocator);
    fclose(fp);
    // PrintTokenList(&allocator, (Node *) OffsetToPtr(&allocator, head_token_offset));
    // fprintf(stdout, "\n\n\tAllocator>>size:%luB>>capacity:%lukb\n\n", allocator->size, allocator->capacity);
    fprintf(stdout, "%s,%lu,%lu,%lu,%lu\n", "tokenise", allocator->size, allocator->capacity, allocator->size / 1024, allocator->capacity / 1024);

    u32 root_obj_offset = PtrToOffset(allocator, AOAlloc(allocator, sizeof(Object)));
    ParseObject(allocator, root_obj_offset, &head_token_offset);
    // PrintObject(stdout, allocator, root_obj_offset);
    // fprintf(stdout, "\n\n\tAllocator>>size:%lukB>>capacity:%lukb\n\n", allocator->size / 1024, allocator->capacity / 1024);
    fprintf(stdout, "%s,%lu,%lu,%lu,%lu\n", "parse", allocator->size, allocator->capacity, allocator->size / 1024, allocator->capacity / 1024);

    PrintObject(stdout, allocator, root_obj_offset);

    return root_obj_offset;
}

static void ParseJsonSep(Json *json, char *filename)
{
    TimeFunction;

    AOInit(&json->a, 4096*100);

    AllocOffset a_token;
    AOInit(&a_token, 4096);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "cannot open file: %s, for reading\n", filename);
    }

    // fprintf(stdout, "%s: memory size\n", filename);
    // fprintf(stdout, "run,sizeB,capacityB,sizekB,capacitykB\n");
    u32 head_token_offset;
    {
        // TimeBlock("Tokenise");
        head_token_offset = ExtractTokens(fp, &a_token);
        fclose(fp);
    }

    // fprintf(stdout, "%s,%lu,%lu,%lu,%lu\n", "tokenise", a_token.size, a_token.capacity, a_token.size / 1024, a_token.capacity / 1024);

    {
        // TimeBlock("Parse");
    json->head_offset = PtrToOffset(&json->a, AOAlloc(&json->a, sizeof(Object)));
    ParseObjectSep(&json->a, json->head_offset, &a_token, &head_token_offset);
    AOFree(&a_token);
    }

    // fprintf(stdout, "%s,%lu,%lu,%lu,%lu\n", "parse", json->a.size, json->a.capacity, json->a.size / 1024, json->a.capacity / 1024);
}
