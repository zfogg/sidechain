# Email Forwarder Lambda Function

This Lambda function receives emails sent to `hi@sidechain.live` and forwards them to a specified email address (currently `zach.fogg@gmail.com`).

## Architecture

The email forwarding pipeline works as follows:

1. **Email Reception**: SES receives an email for `hi@sidechain.live`
2. **S3 Storage**: SES stores the raw email message in S3
3. **SNS Notification**: SES sends an SNS notification to trigger the Lambda
4. **Lambda Processing**: Lambda retrieves the email from S3 and forwards it
5. **SES Delivery**: Lambda uses SES to send the forwarded email to the target address

## Deployment

### Prerequisites

- AWS CLI v2 configured with credentials
- Go 1.21+
- The `bootstrap` binary from AWS Lambda Go runtime
- Terraform (optional, for IaC deployment)

### Build and Deploy

#### Option 1: Manual Deployment (Recommended for Testing)

1. **Build the Lambda binary**:
   ```bash
   cd backend/cmd/email-forwarder
   GOOS=linux GOARCH=arm64 go build -o bootstrap main.go
   ```

2. **Create deployment package**:
   ```bash
   zip lambda.zip bootstrap
   ```

3. **Create IAM Role** (one-time setup):
   ```bash
   # Create trust policy file
   cat > trust-policy.json <<EOF
   {
     "Version": "2012-10-17",
     "Statement": [
       {
         "Effect": "Allow",
         "Principal": {
           "Service": "lambda.amazonaws.com"
         },
         "Action": "sts:AssumeRole"
       }
     ]
   }
   EOF

   # Create role
   aws iam create-role \
     --role-name sidechain-email-forwarder-role \
     --assume-role-policy-document file://trust-policy.json \
     --region us-west-2
   ```

4. **Attach policies to role**:
   ```bash
   # S3 read policy
   aws iam put-role-policy \
     --role-name sidechain-email-forwarder-role \
     --policy-name email-forwarder-s3-policy \
     --policy-document '{
       "Version": "2012-10-17",
       "Statement": [{
         "Effect": "Allow",
         "Action": "s3:GetObject",
         "Resource": "arn:aws:s3:::sidechain-email-receipts/*"
       }]
     }' \
     --region us-west-2

   # SES send policy
   aws iam put-role-policy \
     --role-name sidechain-email-forwarder-role \
     --policy-name email-forwarder-ses-policy \
     --policy-document '{
     "Version": "2012-10-17",
       "Statement": [{
         "Effect": "Allow",
         "Action": ["ses:SendEmail", "ses:SendRawEmail"],
         "Resource": "*"
       }]
     }' \
     --region us-west-2

   # CloudWatch Logs policy
   aws iam put-role-policy \
     --role-name sidechain-email-forwarder-role \
     --policy-name email-forwarder-logs-policy \
     --policy-document '{
       "Version": "2012-10-17",
       "Statement": [{
         "Effect": "Allow",
         "Action": ["logs:CreateLogGroup", "logs:CreateLogStream", "logs:PutLogEvents"],
         "Resource": "arn:aws:logs:us-west-2:*:*"
       }]
     }' \
     --region us-west-2
   ```

5. **Create S3 bucket**:
   ```bash
   aws s3api create-bucket \
     --bucket sidechain-email-receipts \
     --region us-west-2 \
     --create-bucket-configuration LocationConstraint=us-west-2
   ```

6. **Create the Lambda function**:
   ```bash
   aws lambda create-function \
     --function-name sidechain-email-forwarder \
     --runtime provided.al2 \
     --role arn:aws:iam::<ACCOUNT_ID>:role/sidechain-email-forwarder-role \
     --handler email-forwarder \
     --timeout 60 \
     --memory-size 512 \
     --environment Variables="{S3_BUCKET_NAME=sidechain-email-receipts,FORWARD_TO_EMAIL=zach.fogg@gmail.com}" \
     --zip-file fileb://lambda.zip \
     --region us-west-2
   ```

7. **Create SNS topic**:
   ```bash
   aws sns create-topic \
     --name sidechain-email-receipts \
     --region us-west-2
   ```

8. **Subscribe Lambda to SNS**:
   ```bash
   aws sns subscribe \
     --topic-arn arn:aws:sns:us-west-2:<ACCOUNT_ID>:sidechain-email-receipts \
     --protocol lambda \
     --notification-endpoint arn:aws:lambda:us-west-2:<ACCOUNT_ID>:function:sidechain-email-forwarder \
     --region us-west-2

   # Give Lambda permission to be invoked by SNS
   aws lambda add-permission \
     --function-name sidechain-email-forwarder \
     --statement-id AllowSNSInvoke \
     --action lambda:InvokeFunction \
     --principal sns.amazonaws.com \
     --source-arn arn:aws:sns:us-west-2:<ACCOUNT_ID>:sidechain-email-receipts \
     --region us-west-2
   ```

#### Option 2: Terraform Deployment (Recommended for Production)

1. **Enable in Terraform**:
   ```bash
   # Edit terraform/main.tf or create terraform.tfvars
   echo 'email_forwarder_enabled = true' >> terraform/terraform.tfvars
   echo 'forward_to_email = "zach.fogg@gmail.com"' >> terraform/terraform.tfvars
   ```

2. **Build the Lambda binary** (as above)

3. **Deploy with Terraform**:
   ```bash
   cd terraform
   terraform init
   terraform plan
   terraform apply
   ```

### Update Deployment

To update the Lambda function after code changes:

```bash
# Rebuild the binary and package
cd backend/cmd/email-forwarder
GOOS=linux GOARCH=arm64 go build -o bootstrap main.go
zip lambda.zip bootstrap

# Update Lambda
aws lambda update-function-code \
  --function-name sidechain-email-forwarder \
  --zip-file fileb://lambda.zip \
  --region us-west-2
```

## Configuration

### Environment Variables

- `S3_BUCKET_NAME`: S3 bucket where SES stores received emails (default: `sidechain-email-receipts`)
- `FORWARD_TO_EMAIL`: Email address to forward messages to (default: `zach.fogg@gmail.com`)

### SES Configuration

After deployment, configure SES to accept emails for `hi@sidechain.live`:

1. **Verify domain in SES** (if not already done):
   ```bash
   aws ses verify-domain-identity \
     --domain sidechain.live \
     --region us-west-2
   ```

2. **Create SES receipt rule set**:
   ```bash
   aws ses create-receipt-rule-set \
     --rule-set-name sidechain-email-forwarder \
     --region us-west-2

   # Make it active
   aws ses set-active-receipt-rule-set \
     --rule-set-name sidechain-email-forwarder \
     --region us-west-2
   ```

3. **Create receipt rule** (the Terraform configuration does this):
   ```bash
   aws ses put-receipt-rule \
     --rule-set-name sidechain-email-forwarder \
     --rule '{
       "Name": "forward-hi-sidechain",
       "Enabled": true,
       "TlsPolicy": "Optional",
       "Recipients": ["hi@sidechain.live"],
       "Actions": [
         {
           "S3Action": {
             "BucketName": "sidechain-email-receipts",
             "ObjectKeyPrefix": "",
             "TopicArn": "arn:aws:sns:us-west-2:<ACCOUNT_ID>:sidechain-email-receipts"
           }
         },
         {
           "SNSAction": {
             "TopicArn": "arn:aws:sns:us-west-2:<ACCOUNT_ID>:sidechain-email-receipts"
           }
         }
       ],
       "ScanEnabled": true
     }' \
     --region us-west-2
   ```

## Testing

### Send a Test Email

After deployment, send a test email:

```bash
aws ses send-email \
  --from noreply@sidechain.live \
  --destination ToAddresses=hi@sidechain.live \
  --message Subject={Data="Test Email"},Body={Text={Data="This is a test email"}} \
  --region us-west-2 --profile zfogg
```

### Check Lambda Logs

```bash
# Get log streams
aws logs describe-log-streams \
  --log-group-name /aws/lambda/sidechain-email-forwarder \
  --region us-west-2

# View logs
aws logs tail /aws/lambda/sidechain-email-forwarder --region us-west-2 --follow
```

## Troubleshooting

### Lambda not triggered

- Check SNS topic subscriptions: `aws sns list-subscriptions-by-topic --topic-arn <topic-arn>`
- Check Lambda permissions: `aws lambda get-policy --function-name sidechain-email-forwarder`
- Check SES receipt rules: `aws ses describe-receipt-rule-set --rule-set-name sidechain-email-forwarder`

### Email not forwarded

- Check Lambda logs in CloudWatch
- Verify S3 bucket has received the email: `aws s3 ls sidechain-email-receipts/`
- Verify `FORWARD_TO_EMAIL` environment variable is set correctly
- Check SES sending limits (might be in sandbox mode)

### Email forwarding errors

1. **S3 permission denied**: Ensure Lambda role has S3:GetObject permission
2. **SES access denied**: Ensure Lambda role has SES:SendEmail permission
3. **Email parsing failed**: Check raw email format in S3

## Cleanup

To remove all resources:

```bash
# Terraform
terraform destroy

# Or manually:
aws lambda delete-function --function-name sidechain-email-forwarder --region us-west-2
aws sns delete-topic --topic-arn arn:aws:sns:us-west-2:<ACCOUNT_ID>:sidechain-email-receipts --region us-west-2
aws s3 rb s3://sidechain-email-receipts --force
aws iam delete-role-policy --role-name sidechain-email-forwarder-role --policy-name email-forwarder-s3-policy --region us-west-2
aws iam delete-role-policy --role-name sidechain-email-forwarder-role --policy-name email-forwarder-ses-policy --region us-west-2
aws iam delete-role-policy --role-name sidechain-email-forwarder-role --policy-name email-forwarder-logs-policy --region us-west-2
aws iam delete-role --role-name sidechain-email-forwarder-role --region us-west-2
```
