package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"strings"
	"time"

	"github.com/aws/aws-lambda-go/events"
	"github.com/aws/aws-lambda-go/lambda"
	"github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/s3"
	"github.com/aws/aws-sdk-go-v2/service/ses"
	"github.com/aws/aws-sdk-go-v2/service/ses/types"
)

var (
	s3Client   *s3.Client
	sesClient  *ses.Client
	bucketName string
	forwardTo  string
)

func init() {
	var err error
	cfg, err := config.LoadDefaultConfig(context.Background())
	if err != nil {
		log.Fatalf("Failed to load AWS config: %v", err)
	}

	s3Client = s3.NewFromConfig(cfg)
	sesClient = ses.NewFromConfig(cfg)

	bucketName = os.Getenv("S3_BUCKET_NAME")
	forwardTo = os.Getenv("FORWARD_TO_EMAIL")

	if bucketName == "" || forwardTo == "" {
		log.Fatal("S3_BUCKET_NAME and FORWARD_TO_EMAIL environment variables are required")
	}

	log.Printf("Email forwarder initialized - forwarding to: %s", forwardTo)
}

// SESMessage represents the SNS message payload from SES
type SESMessage struct {
	EventSource string `json:"eventSource"`
	Receipt     struct {
		Timestamp            string                 `json:"timestamp"`
		Recipients           []string               `json:"recipients"`
		SpamVerdict          map[string]interface{} `json:"spamVerdict"`
		DkimVerdict          map[string]interface{} `json:"dkimVerdict"`
		ProcessingTimeMillis int                    `json:"processingTimeMillis"`
		SPFVerdict           map[string]interface{} `json:"spfVerdict"`
		Action               map[string]interface{} `json:"action"`
		DmarcVerdict         map[string]interface{} `json:"dmarcVerdict"`
	} `json:"receipt"`
	Mail struct {
		Timestamp   string `json:"timestamp"`
		Destination []string `json:"destination"`
		Headers     []struct {
			Name  string `json:"name"`
			Value string `json:"value"`
		} `json:"headers"`
		CommonHeaders struct {
			ReturnPath string   `json:"returnPath"`
			From       []string `json:"from"`
			Date       string   `json:"date"`
			To         []string `json:"to"`
			MessageID  string   `json:"messageId"`
			Subject    string   `json:"subject"`
		} `json:"commonHeaders"`
		Source    string `json:"source"`
		MessageID string `json:"messageId"`
	} `json:"mail"`
}

func HandleSESEvent(ctx context.Context, snsEvent events.SNSEvent) error {
	for _, record := range snsEvent.Records {
		sns := record.SNS

		// Parse the SNS message
		var sesMsg SESMessage
		if err := json.Unmarshal([]byte(sns.Message), &sesMsg); err != nil {
			log.Printf("Error parsing SES message: %v", err)
			continue
		}

		log.Printf("Processing email from: %s, MessageID: %s", sesMsg.Mail.Source, sesMsg.Mail.MessageID)

		// Get the raw email from S3
		// SES stores emails with the messageId as the key
		messageID := sesMsg.Mail.MessageID
		if len(messageID) == 0 {
			log.Printf("Empty message ID, skipping")
			continue
		}
		key := messageID

		// Retry logic for S3 GetObject (may have eventual consistency delay)
		var result *s3.GetObjectOutput
		var err error
		for attempt := 1; attempt <= 3; attempt++ {
			result, err = s3Client.GetObject(ctx, &s3.GetObjectInput{
				Bucket: &bucketName,
				Key:    &key,
			})
			if err == nil {
				break
			}
			if attempt < 3 {
				log.Printf("Attempt %d to retrieve email from S3 failed: %v. Retrying...", attempt, err)
				time.Sleep(time.Duration(attempt*500) * time.Millisecond)
			}
		}
		if err != nil {
			log.Printf("Error retrieving email from S3 after retries: %v", err)
			continue
		}
		defer result.Body.Close()

		// Read the raw email
		emailData, err := io.ReadAll(result.Body)
		if err != nil {
			log.Printf("Error reading email data: %v", err)
			continue
		}

		// Forward the email
		if err := forwardEmail(ctx, sesMsg, string(emailData)); err != nil {
			log.Printf("Error forwarding email: %v", err)
			continue
		}

		log.Printf("Successfully forwarded email from: %s", sesMsg.Mail.Source)
	}

	return nil
}

func forwardEmail(ctx context.Context, mail SESMessage, rawEmail string) error {
	// Extract email components
	subject := mail.Mail.CommonHeaders.Subject
	from := mail.Mail.Source

	// Prepend "Fwd:" if not already present
	if !strings.HasPrefix(subject, "Fwd:") {
		subject = fmt.Sprintf("Fwd: %s", subject)
	}

	// Create the forwarded email body
	htmlBody := fmt.Sprintf(`
<!DOCTYPE html>
<html>
<head>
	<meta charset="UTF-8">
	<style>
		body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
		.forwarded-header {
			background-color: #f5f5f5;
			padding: 10px;
			border-left: 3px solid #00d4ff;
			margin: 20px 0;
		}
		.forwarded-content { margin-top: 20px; }
	</style>
</head>
<body>
	<div class="forwarded-header">
		<strong>Forwarded from hi@sidechain.live</strong><br>
		From: %s<br>
		Date: %s<br>
		Subject: %s
	</div>
	<div class="forwarded-content">
		<!-- Original email content will be inserted here -->
		<pre style="white-space: pre-wrap; word-wrap: break-word;">%s</pre>
	</div>
</body>
</html>
	`, from, mail.Mail.CommonHeaders.Date, mail.Mail.CommonHeaders.Subject, rawEmail)

	textBody := fmt.Sprintf(`
---------- Forwarded message ---------
From: %s
Date: %s
Subject: %s

%s
	`, from, mail.Mail.CommonHeaders.Date, mail.Mail.CommonHeaders.Subject, rawEmail)

	// Send the forwarded email
	input := &ses.SendEmailInput{
		Source: &forwardTo,
		Destination: &types.Destination{
			ToAddresses: []string{forwardTo},
		},
		Message: &types.Message{
			Subject: &types.Content{
				Data:    &subject,
				Charset: func() *string { s := "UTF-8"; return &s }(),
			},
			Body: &types.Body{
				Html: &types.Content{
					Data:    &htmlBody,
					Charset: func() *string { s := "UTF-8"; return &s }(),
				},
				Text: &types.Content{
					Data:    &textBody,
					Charset: func() *string { s := "UTF-8"; return &s }(),
				},
			},
		},
	}

	_, err := sesClient.SendEmail(ctx, input)
	return err
}

func main() {
	lambda.Start(HandleSESEvent)
}
